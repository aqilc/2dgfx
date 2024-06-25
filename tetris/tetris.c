#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <2dgfx.h>

#define VEC_H_IMPLEMENTATION
#define VEC_H_STATIC_INLINE
#include <vec.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

#define PIECE_I 1
#define PIECE_T 2
#define PIECE_L 3
#define PIECE_J 4
#define PIECE_S 5
#define PIECE_Z 6
#define PIECE_O 7

#define PIECE_START 4, -2



// --------------------------------- Structs and Constants --------------------------------- //

struct tris_board {
  char* board;
  gfx_vector_micro size;
  gfx_vector_micro pos; // player's position
  u8 top_buffer;
  struct {
    u8 rotation;
    u8 type;
    u8 hold;
    u8 held_for_piece;
    u8 queue[7 * 2];
    u8 queue_len;
  };
};

struct tris_settings {
  u16 ARR; // Automatic Repeat Rate
  u16 DAS; // Delayed Auto Shift, in ms steps by frames  (16.6ms)
  u16 DCD; // DAS Cut Delay
  u16 SDF; // Soft Drop Factor, from 5x to 40x -> Infinity

  // Keybinds
  u16 left;
  u16 right;
  u16 down;
  u16 harddrop;
  u16 rotate_cw;
  u16 rotate_ccw;
  u16 rotate_180;
  u16 hold;
  u16 reset;
} settings = {
  .ARR = 0,
  .DAS = 110,
  .DCD = 0,
  .SDF = 41,
  .left = GFX_KEY_LEFT,
  .right = GFX_KEY_RIGHT,
  .down = GFX_KEY_DOWN,
  .harddrop = GFX_KEY_SPACE,
  .rotate_cw = GFX_KEY_UP,
  .rotate_ccw = GFX_KEY_S,
  .rotate_180 = GFX_KEY_A,
  .hold = GFX_KEY_D,
  .reset = GFX_KEY_R,
};

struct {

  // DAS state
  double das_start;
  int das_dir;
  bool left_first;
  bool das_done;

  
} tris_state = {0};

const gfx_vector_micro tris_pieces_places[][4][4] = { {},
  { { 0,1, 1,1, 2,1, 3,1 }, { 2,0, 2,1, 2,2, 2,3 }, { 0,2, 1,2, 2,2, 3,2 }, { 1,0, 1,1, 1,2, 1,3 } }, // I
  { { 1,0, 0,1, 1,1, 2,1 }, { 1,0, 1,1, 2,1, 1,2 }, { 0,1, 1,1, 2,1, 1,2 }, { 1,0, 0,1, 1,1, 1,2 } }, // T
  { { 2,0, 0,1, 1,1, 2,1 }, { 1,0, 1,1, 1,2, 2,2 }, { 0,1, 1,1, 2,1, 0,2 }, { 0,0, 1,0, 1,1, 1,2 } }, // L
  { { 0,0, 0,1, 1,1, 2,1 }, { 1,0, 2,0, 1,1, 1,2 }, { 0,1, 1,1, 2,1, 2,2 }, { 1,0, 1,1, 0,2, 1,2 } }, // J
  { { 1,0, 2,0, 0,1, 1,1 }, { 1,0, 1,1, 2,1, 2,2 }, { 1,1, 2,1, 0,2, 1,2 }, { 0,0, 0,1, 1,1, 1,2 } }, // S
  { { 0,0, 1,0, 1,1, 2,1 }, { 2,0, 1,1, 2,1, 1,2 }, { 0,1, 1,1, 1,2, 2,2 }, { 1,0, 0,1, 1,1, 0,2 } }, // Z
  { { 1,0, 2,0, 1,1, 2,1 }, { 1,0, 2,0, 1,1, 2,1 }, { 1,0, 2,0, 1,1, 2,1 }, { 1,0, 2,0, 1,1, 2,1 } }, // O
};


#pragma push_macro("X")
#define X 255 // Value that would break the program if read and treated seriously. Meant to.
const struct {
  u8 leftbottomright[3][4][4]; // Four rotation states, each with four values for each block
  u8 top_limit[4]; u8 bottom_limit[4]; u8 left_limit[4]; u8 right_limit[4];
} tris_pieces_left_right_bottom[] = { {},
  { { X,0,X,X, 2,2,2,2, X,X,0,X, 1,1,1,1,   1,1,1,1, X,X,3,X, 2,2,2,2, X,3,X,X,   X,3,X,X, 2,2,2,2, X,X,3,X, 1,1,1,1 }, { 1,0,2,0 }, { 1,3,2,3 }, { 0,2,0,1 }, { 3,2,3,1 } }, // I
  { { 1,0,X,X, 1,1,1,X, X,0,1,X, 1,0,1,X,   1,1,1,X, X,2,1,X, 1,2,1,X, 1,2,X,X,   1,2,X,X, 1,2,1,X, X,2,1,X, 1,1,1,X }, { 0,0,1,0 }, { 1,2,2,2 }, { 0,1,0,0 }, { 2,2,2,1 } }, // T
  { { 2,0,X,X, 1,1,1,X, X,0,0,X, 0,1,1,X,   1,1,1,X, X,2,2,X, 2,1,1,X, 0,2,X,X,   2,2,X,X, 1,1,2,X, X,2,0,X, 1,1,1,X }, { 0,0,1,0 }, { 1,2,2,2 }, { 0,1,0,0 }, { 2,2,2,1 } }, // L
  { { 0,0,X,X, 1,1,1,X, X,0,2,X, 1,1,0,X,   1,1,1,X, X,2,0,X, 1,1,2,X, 2,2,X,X,   0,2,X,X, 2,1,1,X, X,2,2,X, 1,1,1,X }, { 0,0,1,0 }, { 1,2,2,2 }, { 0,1,0,0 }, { 2,2,2,1 } }, // J
  { { 1,0,X,X, 1,1,2,X, X,1,0,X, 0,0,1,X,   1,1,0,X, X,1,2,X, 2,2,1,X, 1,2,X,X,   2,1,X,X, 1,2,2,X, X,2,1,X, 0,1,1,X }, { 0,0,1,0 }, { 1,2,2,2 }, { 0,1,0,0 }, { 2,2,2,1 } }, // S
  { { 0,1,X,X, 2,1,1,X, X,0,1,X, 1,0,0,X,   0,1,1,X, X,2,1,X, 1,2,2,X, 2,1,X,X,   1,2,X,X, 2,2,1,X, X,1,2,X, 1,1,0,X }, { 0,0,1,0 }, { 1,2,2,2 }, { 0,1,0,0 }, { 2,2,2,1 } }, // Z
  { { 1,1,X,X, 1,1,X,X, 1,1,X,X, 1,1,X,X,   X,1,1,X, X,1,1,X, X,1,1,X, X,1,1,X,   2,2,X,X, 2,2,X,X, 2,2,X,X, 2,2,X,X }, { 0,0,0,0 }, { 1,1,1,1 }, { 1,1,1,1 }, { 2,2,2,2 } }, // O
};
#pragma pop_macro("X")


const gfx_vector_micro tris_jlstz_rot_offsets[8][4] = {
  { {  1, 0 }, {  1,-1 }, {  0, 2 }, {  1, 2 } }, // 0 >> 3
  { { -1, 0 }, { -1,-1 }, {  0, 2 }, { -1, 2 } }, // 0 >> 1
  { {  1, 0 }, {  1, 1 }, {  0,-2 }, {  1,-2 } }, // 1 >> 0
  { {  1, 0 }, {  1, 1 }, {  0,-2 }, {  1,-2 } }, // 1 >> 2
  { { -1, 0 }, { -1,-1 }, {  0, 2 }, { -1, 2 } }, // 2 >> 1
  { {  1, 0 }, {  1,-1 }, {  0, 2 }, {  1, 2 } }, // 2 >> 3
  { { -1, 0 }, { -1, 1 }, {  0,-2 }, { -1,-2 } }, // 3 >> 2
  { { -1, 0 }, { -1, 1 }, {  0,-2 }, { -1,-2 } }, // 3 >> 0
};

const gfx_vector_micro tris_i_rot_offsets[8][4] = {
  { { -2, 0 }, {  1, 0 }, { -2, 1 }, {  1,-2 } },
  { {  2, 0 }, { -1, 0 }, {  2,-1 }, { -1, 2 } },
  { { -1, 0 }, {  2, 0 }, { -1,-2 }, {  2, 1 } },
  { {  1, 0 }, { -2, 0 }, {  1, 2 }, { -2,-1 } },
  { {  2, 0 }, { -1, 0 }, {  2,-1 }, { -1, 2 } },
  { { -2, 0 }, {  1, 0 }, { -2, 1 }, {  1,-2 } },
  { {  1, 0 }, { -2, 0 }, {  1, 2 }, { -2,-1 } },
  { { -1, 0 }, {  2, 0 }, { -1,-2 }, {  2, 1 } },
};

const gfx_vector_micro tris_jlstz_rot180_offsets[][11] = {
  { {  1, 0 }, {  2, 0 }, {  1, 1 }, {  2, 1 }, { -1, 0 }, { -2, 0 }, { -1, 1 }, { -2, 1 }, {  0,-1 }, {  3, 0 }, { -3, 0 } }, // 0>>2─┐
  { {  0, 1 }, {  0, 2 }, { -1, 1 }, { -1, 2 }, {  0,-1 }, {  0,-2 }, { -1,-1 }, { -1,-2 }, {  1, 0 }, {  0, 3 }, {  0,-3 } }, // 1>>3─┼┐
  { { -1, 0 }, { -2, 0 }, { -1,-1 }, { -2,-1 }, {  1, 0 }, {  2, 0 }, {  1,-1 }, {  2,-1 }, {  0, 1 }, { -3, 0 }, {  3, 0 } }, // 2>>0─┘│
  { {  0, 1 }, {  0, 2 }, {  1, 1 }, {  1, 2 }, {  0,-1 }, {  0,-2 }, {  1,-1 }, {  1,-2 }, { -1, 0 }, {  0, 3 }, {  0,-3 } }, // 3>>1──┘
};
const gfx_vector_micro tris_i_rot180_offsets[][5] = {
  { { -1, 0 }, { -2, 0 }, {  1, 0 }, {  2, 0 }, {  0,-1 } }, // 0>>2─┐ flipped
  { {  0,-1 }, {  0, 2 }, {  0, 1 }, {  0,-2 }, { -1, 0 } }, // 1>>3─┼┐
  { {  1, 0 }, {  2, 0 }, { -1, 0 }, { -2, 0 }, {  0, 1 } }, // 2>>0─┘│
  { {  0,-1 }, {  0, 2 }, {  0, 1 }, {  0,-2 }, {  1, 0 } }, // 3>>1──┘
};

const u8 tris_block_colors[][2][3] = {
  { {  54,  57,  65 }, {   0,   0,   0 } }, // Empty
  { {  72, 176, 223 }, { 114, 235, 254 } }, // I
  { { 185,  69, 198 }, { 248,  96, 255 } }, // T
  { { 241, 136,  53 }, { 253, 185,  97 } }, // L
  { {  25, 102, 179 }, {  58, 156, 252 } }, // J
  { {  85, 184,  83 }, { 136, 248, 134 } }, // S
  { { 233,  76, 103 }, { 253, 125, 123 } }, // Z
  { { 245, 207,  75 }, { 255, 255, 135 } }, // O
};

const char tris_piece_to_char[] = { ' ', 'I', 'T', 'L', 'J', 'S', 'Z', 'O' };

static inline const u8 char_to_piece_idx(char c) {
  switch (c) {
  case 'i': case 'I': return 1;
  case 't': case 'T': return 2;
  case 'l': case 'L': return 3;
  case 'j': case 'J': return 4;
  case 's': case 'S': return 5;
  case 'z': case 'Z': return 6;
  case 'o': case 'O': return 7;
  default: return 0;
  }
}









// --------------------------------- Graphics and Game Logic --------------------------------- //



static void fill_queue(struct tris_board* board) {
  if(board->queue_len > 7) return;

  board->queue[7] = board->queue[0];
  board->queue[8] = board->queue[1];
  board->queue[9] = board->queue[2];
  board->queue[10] = board->queue[3];
  board->queue[11] = board->queue[4];
  board->queue[12] = board->queue[5];
  board->queue[13] = board->queue[6];
  
  u8 pieces_used = 0; // bitfield for used pieces
  for(u8 i = 0; i < 6; i++) {
    i8 nth_piece = rand() % (7 - i) + 1;
    u8 pieces_not_used = ~pieces_used & 0b01111111;
    // printf("Picking piece %d, used: %d, not used: %d\n", nth_piece, pieces_used, pieces_not_used);
    while(-- nth_piece > 0) // printf("clz: %d %d | ", pieces_not_used, (6+(25 - __builtin_clzl(pieces_not_used)))),
      pieces_not_used ^= 1 << (6 + (25 - __builtin_clzl(pieces_not_used)));
    u8 piece = __builtin_clzl(pieces_not_used) - 24;
    // printf("%d: %d, 0x%X\n", i, piece, pieces_not_used);
    
    board->queue[i] = piece;
    pieces_used |= 1 << (7 - piece);
  }
  board->queue[6] = __builtin_clzl(~pieces_used & 0b01111111) - 24;
  
  board->queue_len += 7;
  // printf("Queue: %d, %d, %d, %d, %d, %d, %d\n", board->queue[0], board->queue[1], board->queue[2], board->queue[3], board->queue[4], board->queue[5], board->queue[6]);
}

static inline void pop_queue(struct tris_board* board) {
  if(board->queue_len < 8) fill_queue(board);
  board->pos = (gfx_vector_micro) { PIECE_START };
  board->rotation = 0;
  board->type = board->queue[-- board->queue_len];
}

static void clear_board(struct tris_board* board) {
  memset(board->board, 0, board->size.x * (board->size.y + board->top_buffer));
  board->hold = 0;
  board->held_for_piece = false;
  board->rotation = 0;
  board->queue_len = 0;
  fill_queue(board);
  pop_queue(board);
}

struct tris_board* tris_new_board();
void tris_grid(int x, int y, int size, int width, int height, int thickness);
void tris_draw_board(struct tris_board* board, int blocksize, int x, int y);
static u32 project_sideways(struct tris_board* board, int dir);
static u32 project_down(struct tris_board* board);

_Thread_local struct tris_board* controlled = NULL;






void loop();

double last_frame_begin = 0.0;
char* fps = NULL;
int main() {
  gfx_init("Tetris", 1000, 1000);
  controlled = tris_new_board();
  gfx_face face = gfx_load_font("roboto.ttf");
  while(gfx_next_frame()) {
    last_frame_begin = gfx_time();
    loop();

    fontsize(20);
    if(gfx_fpschanged())
      fps = vfmt("%.2f fps", gfx_fps());
    if(fps) text(fps, 1000 - 150, 1000 - 10);

    const double now = gfx_time();
    double res;
    if(!tris_state.das_done && tris_state.das_start &&
        now - last_frame_begin < (1.0 / 60.0) &&
        // printf("got here %lf, %lf, %lf\n", tris_state.das_start, now, (tris_state.das_start * 1000.0 + settings.DAS) / 1000.0 - now ) &&
        (res = (tris_state.das_start * 1000 + settings.DAS) / 1000 - now) < (1.0 / 60.0)) {
      if((res *= 1000.0) > 0.5) gfx_sleep(round(res));
      controlled->pos.x = project_sideways(controlled, tris_state.das_dir);
      tris_state.das_done = true;
      // printf("DAS Fired at %lf time after %d dir\n", gfx_time() - tris_state.das_start, tris_state.das_dir);
    }
    gfx_frameend();
  }
}

void loop() {
  tris_draw_board(controlled, 40, 250, 100);
}

struct tris_board* tris_new_board() {
  struct tris_board* board = malloc(sizeof(struct tris_board));
  board->board = calloc(24 * 10, sizeof(char));
  board->size = (gfx_vector_micro) { .x = 10, .y = 20 };
  board->top_buffer = 4;
  board->rotation = 0;
  board->queue_len = 0;
  board->hold = 0;
  fill_queue(board);
  pop_queue(board);
  return board;
}

void tris_draw_block(char block, char rot, int blocksize, int x, int y) {
  if(!block) return;
  
  fill(tris_block_colors[block][1][0], tris_block_colors[block][1][1], tris_block_colors[block][1][2], 255);
  for(int i = 0; i < 4; i ++)
    rect(x + tris_pieces_places[block][rot][i].x * blocksize, y + tris_pieces_places[block][rot][i].y * blocksize - blocksize / 5, blocksize, blocksize);

  fill(tris_block_colors[block][0][0], tris_block_colors[block][0][1], tris_block_colors[block][0][2], 255);
  for(int i = 0; i < 4; i ++)
    rect(x + tris_pieces_places[block][rot][i].x * blocksize, y + tris_pieces_places[block][rot][i].y * blocksize, blocksize, blocksize);
}

void tris_draw_shadow(char block, char rot, int blocksize, int x, int y) {
  if(!block) return;
  
  fill(tris_block_colors[block][0][0], tris_block_colors[block][0][1], tris_block_colors[block][0][2], 100);
  for(int i = 0; i < 4; i ++)
    rect(x + tris_pieces_places[block][rot][i].x * blocksize, y + tris_pieces_places[block][rot][i].y * blocksize - blocksize / 5, blocksize, blocksize);
}

void tris_draw_board(struct tris_board* board, int blocksize, int x, int y) {
  if(!board || !board->board) return;
  
  int bw = board->size.w;
  int bh = board->size.h;
  int buffer = board->top_buffer;
  
  // fill(0, 0, 0, 255);
  // rect(x, y, bw * blocksize, bh * blocksize);

  int line_width = blocksize / 10;
  fill(255, 255, 255, 255);
  rect(x - line_width, y, line_width, bh * blocksize);
  rect(x + bw * blocksize, y, line_width, bh * blocksize);
  rect(x, y + bh * blocksize, bw * blocksize, line_width);
  fill(150, 150, 150, 255);
  rect(x, y + bh * blocksize - blocksize / 5, bw * blocksize, blocksize / 5);

  fill(255, 255, 255, 255);
  int rightside = x + bw * blocksize;
  int diag_x_left = blocksize * 4 + blocksize / 2;
  int diag_x_right = blocksize * 5 - line_width * 2;
  int diag_y_top = blocksize * 15 + blocksize / 2;
  int diag_y_bottom = blocksize * 16;
  rect(rightside + line_width, y, diag_x_right, blocksize);
  rect(rightside + line_width + diag_x_right, y, line_width, diag_y_top);
  rect(rightside + line_width, y + diag_y_bottom, diag_x_left, line_width);
  quad(rightside + line_width + diag_x_left, y + diag_y_bottom,
       rightside + line_width + diag_x_right, y + diag_y_top,
       rightside + line_width + diag_x_right + line_width, y + diag_y_top,
       rightside + line_width + diag_x_left, y + diag_y_bottom + line_width);


  // NEXT text
  int text_height = blocksize * 4 / 5;
  int text_top = y + line_width;
  fill(0, 0, 0, 255);

  // N
  int n_left = blocksize / 5;
  int n_right = blocksize * 5 / 8;
  rect(rightside + n_left,  text_top, line_width, text_height);
  rect(rightside + n_right, text_top, line_width, text_height);
  quad(rightside + n_left,  text_top,
       rightside + n_right, text_top + text_height,
       rightside + n_right + line_width, text_top + text_height,
       rightside + n_left + line_width, text_top);

  // E
  int e_left = blocksize * 13 / 16;
  int e_width = blocksize * 9 / 20;
  rect(rightside + e_left + line_width, text_top, line_width, text_height);
  rect(rightside + e_left + line_width, text_top, e_width, line_width);
  rect(rightside + e_left + line_width, text_top + text_height / 2 - line_width / 2, e_width - line_width * 3 / 2, line_width);
  rect(rightside + e_left + line_width, text_top + text_height - line_width, e_width, line_width);

  // X
  int x_left = blocksize * 3 / 2;
  int x_width = blocksize * 3 / 8;
  quad(rightside + x_left,  text_top,
       rightside + x_left + line_width, text_top,
       rightside + x_left + x_width + line_width, text_top + text_height,
       rightside + x_left + x_width, text_top + text_height);
  quad(rightside + x_left,  text_top + text_height,
       rightside + x_left + line_width, text_top + text_height,
       rightside + x_left + x_width + line_width, text_top,
       rightside + x_left + x_width, text_top);

  // T
  int t_left = blocksize * 21 / 10;
  int t_width = blocksize / 2;
  rect(rightside + t_left, text_top, t_width, line_width);
  rect(rightside + t_left + t_width / 2 - line_width / 2, text_top, line_width, text_height);

  int hit_cols = 0;

  for (int i = 0; i < bw * (bh + buffer); i ++) {
    if(!board->board[i]) {
      hit_cols &= ~(1 << (i % bw));
      continue;
    }
    char const block = board->board[i];
    
    // Calculates x and y based on just the i value
    int rect_x = i % bw * blocksize + x;
    int rect_y = (i / bw - buffer) * blocksize + y;

    if((hit_cols & (1 << (i % bw))) == 0) {
      fill(tris_block_colors[block][1][0], tris_block_colors[block][1][1], tris_block_colors[block][1][2], 255);
      rect(rect_x, rect_y - blocksize / 5, blocksize, blocksize);
      hit_cols |= 1 << (i % bw);
    }

    fill(tris_block_colors[block][0][0], tris_block_colors[block][0][1], tris_block_colors[block][0][2], 255);
    rect(rect_x, rect_y, blocksize, blocksize);

    // Draws an outline around the block
    // fill(tris_block_colors[block][0][0], tris_block_colors[block][0][1], tris_block_colors[block][0][2], 255);
    // rect(rect_x, rect_y, 1, blocksize);
    // rect(rect_x + blocksize - 1, rect_y, 1, blocksize);
    // rect(rect_x, rect_y, blocksize, 1);
    // rect(rect_x, rect_y + blocksize - 1, blocksize, 1);
  }

  tris_draw_block(board->type, board->rotation, blocksize, x + board->pos.x * blocksize, y + board->pos.y * blocksize);
  tris_draw_block(board->hold, 0, blocksize, x - blocksize * 5, y + blocksize / 2);
  tris_draw_shadow(board->type, board->rotation, blocksize, x + board->pos.x * blocksize, y + project_down(board) * blocksize);

  // Draws the queue
  for(int i = 0; i < 5; i ++) {
    int block = board->queue[board->queue_len - i - 1];
    tris_draw_block(block, 0, blocksize, x + bw * blocksize + blocksize - (block == PIECE_I || block == PIECE_O ? blocksize / 2 : 0),
                    y + blocksize * 3 / 2 + i * (blocksize * 3) - (block == PIECE_I ? blocksize / 2 : 0));
  }
}

void tris_grid(int x, int y, int size, int width, int height, int thickness) {
  for(int i = 1; i < width; i++)
    rect(x + i * size, y, thickness, height * size);
  for(int i = 1; i < height; i++)
    rect(x, y + i * size, width * size, thickness);
}












static inline bool collide(struct tris_board* board, int x, int y, int rot) {
  for(int i = 0; i < 4; i ++) {
    int bx = x + tris_pieces_places[board->type][rot][i].x;
    int by = y + tris_pieces_places[board->type][rot][i].y + board->top_buffer;
    if(bx < 0 || bx >= board->size.x || by >= board->size.h + board->top_buffer) return true;
    if(board->board[by * board->size.x + bx]) return true;
  }
  return false;
}

static u32 project_sideways(struct tris_board* board, int dir /* -1 or 1 or infinite loop :skull: */) {
  // printf("Piece: %c | %d, %d, [%d, %d, %d, %d]\n", tris_piece_to_char[board->type], piece_top, piece_bottom, piece_side[0], piece_side[1], piece_side[2], piece_side[3]);
  int board_limit_to = (board->size.w + dir * board->size.w) / 2;

  int top = tris_pieces_left_right_bottom[board->type].top_limit[board->rotation] + board->pos.y;
  int bottom = tris_pieces_left_right_bottom[board->type].bottom_limit[board->rotation] + board->pos.y;

  printf("%c: %d, %d, %d, %d\n", tris_piece_to_char[board->type], top, bottom, board->pos.x, board_limit_to);
  int lowest_difference = dir * (int) board->size.w;
  for(int y = top; y <= bottom; y ++) {
    int row_start = tris_pieces_left_right_bottom[board->type].leftbottomright[dir + 1][board->rotation][y - board->pos.y] + board->pos.x;
    printf("%d Row start: %d | ", y, row_start);
    bool collision = false;
    for(int x = row_start; x != board_limit_to; x += dir)
      if(board->board[(y + board->top_buffer) * board->size.w + x] &&
        (collision = true) &&
        printf("collision at %d | ", x) &&
        (dir == 1 ? x - (row_start - board->pos.x) < lowest_difference - dir : x - (row_start - board->pos.x) - dir > lowest_difference)) {
        lowest_difference = x - (row_start - board->pos.x) - dir;
      }
    int newval = board_limit_to - (row_start - board->pos.x);
    if(!collision && (dir == 1 ? (--newval) < lowest_difference : newval > lowest_difference)) lowest_difference = newval;
  }
  printf("\ndifference: %d\n", lowest_difference);
  
  // for(int i = board->pos.x + piece_side; i >= 0 && i < board->size.x; i += dir)
  //   for(int j = board->pos.y + piece_bottom; j <= board->pos.y + piece_top; j ++)
  //     if(board->board[j * board->size.x + i]) return i;

  // int width = right - left + 1;
  return lowest_difference;
  // return (board->size.w / 2) + dir * (board->size.w / 2) + (- width - dir * width) / 2 - left;
}

static u32 project_down(struct tris_board* board) {
  int board_limit_to = board->size.h;
  
  int left = tris_pieces_left_right_bottom[board->type].left_limit[board->rotation] + board->pos.x;
  int right = tris_pieces_left_right_bottom[board->type].right_limit[board->rotation] + board->pos.x;

  int lowest_difference = board_limit_to;
  for(int x = left; x <= right; x ++) {
    int col_start = tris_pieces_left_right_bottom[board->type].leftbottomright[1][board->rotation][x - board->pos.x] + board->pos.y;
    bool collision = false;
    int offset = col_start - board->pos.y + 1;
    for(int y = col_start; y < board_limit_to; y ++)
      if(board->board[(y + board->top_buffer) * board->size.w + x] && (collision = true) && y - offset < lowest_difference) {
        lowest_difference = y - offset;
      }
    if(!collision && board_limit_to - offset < lowest_difference) lowest_difference = board_limit_to - offset;
  }

  return lowest_difference;
}

static inline void rotate(struct tris_board* board, int dir) {
  int old_rot = board->rotation;
  int new_rot = (board->rotation + dir) % 4;
  if(new_rot < 0) new_rot += 4;
  if(!collide(board, board->pos.x, board->pos.y, new_rot)) goto complete_rotation;
  // else {
  //   int i = 0;
  //   for(; i < 4; i ++) {
  //     int bx = board->pos.x + tris_pieces_places[board->type][new_rot][i].x;
  //     int by = board->pos.y + tris_pieces_places[board->type][new_rot][i].y + board->top_buffer;
  //     if(bx < 0 || bx >= board->size.x || by >= board->size.h) break;
  //     if(board->board[by * board->size.x + bx]) break;
  //   }
  //   printf("Collide at %d, %d, checking (%d, %d) which is %c\n", board->pos.x, board->pos.y, board->pos.x + tris_pieces_places[board->type][new_rot][i].x,
  //     board->pos.y + tris_pieces_places[board->type][new_rot][i].y,
  //     tris_piece_to_char[board->board[(board->pos.y + tris_pieces_places[board->type][new_rot][i].y + board->top_buffer) * board->size.w + (board->pos.x + tris_pieces_places[board->type][new_rot][i].x)]]);
  // }
  if(board->type == PIECE_O) return;

  gfx_vector_micro* positions = (gfx_vector_micro*) tris_jlstz_rot_offsets[old_rot * 2 + (dir == 1)];
  int len = 4;
  if(board->type == PIECE_I) {
    if(dir < 2) positions = (gfx_vector_micro*) tris_i_rot_offsets[old_rot * 2 + (dir == 1)];
    else positions = (gfx_vector_micro*) tris_i_rot180_offsets[old_rot], len = 5;
  } else if(dir == 2) {
    positions = (gfx_vector_micro*) tris_jlstz_rot180_offsets[old_rot], len = 11;
  }
  // printf("For piece %c, in rotation state %d, new state %d picked idx %d\n", tris_piece_to_char[board->type], old_rot, new_rot, old_rot * 2 + (dir == 1));
  for(int i = 0; i < len; i ++) {
    if(!collide(board, board->pos.x + positions[i].x, board->pos.y + positions[i].y, new_rot)) {
      board->pos.x += positions[i].x;
      board->pos.y += positions[i].y;
      goto complete_rotation;
    }
    //else printf("Collide at %d (%d), %d (%d)\n", board->pos.x + positions[i].x, positions[i].x, board->pos.y + positions[i].y, positions[i].y);
  }
  return;
complete_rotation:
  board->rotation = new_rot;
  if(tris_state.das_start && tris_state.das_done == true)
    controlled->pos.x = project_sideways(controlled, tris_state.das_dir);
}

static void clear_lines(struct tris_board* board) {
  int current_top = INT_MIN;
  int current_region_start = INT_MIN;
  for(int y = 0; y < board->size.h + board->top_buffer; y ++) {
    bool full = true;
    bool empty = true;
    for(int x = 0; x < board->size.w; x ++) {
      if(!board->board[y * board->size.w + x])
        full = false;
      else empty = false;
      if(!full && !empty) break;
    }
    if(empty && current_top == INT_MIN) continue;
    else if (current_top == INT_MIN) current_top = y;
    
    if(full && current_region_start == INT_MIN) current_region_start = y;
    else if(!full && current_region_start != INT_MIN) {
      int non_cleared_lines_size = current_region_start - current_top;
      int cleared_region_size = y - current_region_start;
      // printf("Y: %d, Current_region_start: %d, current_top: %d, Non-cleared: %d, Cleared: %d\n", y, current_region_start, current_top, non_cleared_lines_size, cleared_region_size);
      if(non_cleared_lines_size)
        memmove(board->board + (y - non_cleared_lines_size) * board->size.w,
                board->board + current_top * board->size.w,
                board->size.w * non_cleared_lines_size);
      memset(board->board + current_top * board->size.w, 0, board->size.w * cleared_region_size);
      current_region_start = INT_MIN;
      current_top += cleared_region_size;
    }
  }
  if(current_region_start != INT_MIN) {
    int non_cleared_lines_size = current_region_start - current_top;
    int cleared_region_size = (board->size.h + board->top_buffer) - current_region_start;
    // printf("Y: %d, Current_region_start: %d, current_top: %d, Non-cleared: %d, Cleared: %d\n", (board->size.h + board->top_buffer), current_region_start, current_top, non_cleared_lines_size, cleared_region_size);
    if(non_cleared_lines_size)
      memmove(board->board + ((board->size.h + board->top_buffer) - non_cleared_lines_size) * board->size.w,
              board->board + current_top * board->size.w,
              board->size.w * non_cleared_lines_size);
    memset(board->board + current_top * board->size.w, 0, board->size.w * cleared_region_size);
  }
}

static inline void place(struct tris_board* board) {
  for(int i = 0; i < 4; i ++) {
    gfx_vector_micro pos = tris_pieces_places[board->type][board->rotation][i];
    board->board[(board->pos.y + pos.y + board->top_buffer) * board->size.x + (board->pos.x + pos.x)] = board->type;
  }
  clear_lines(board);
  pop_queue(board);
  board->held_for_piece = false;
  if(collide(board, board->pos.x, board->pos.y, board->rotation))
    clear_board(board);
}




// --------------------------------- Event Handling --------------------------------- //

void on_key_button(gfx_keycode key, bool pressed, gfx_keymod mods) {
  static int direction;
  if((key == settings.left && (direction = -1)) ||
     (key == settings.right && (direction = 1))) {
    if(pressed) { // && !collide(controlled, controlled->pos.x + direction, controlled->pos.y, controlled->rotation)) {
      if(!collide(controlled, controlled->pos.x + direction, controlled->pos.y, controlled->rotation))
        controlled->pos.x += direction;

      if(!tris_state.das_start)
        tris_state.left_first = key == settings.left;
      tris_state.das_start = gfx_time();
      tris_state.das_dir = direction;
      tris_state.das_done = false;
    } else if(tris_state.das_start) { // means key was released + there was ongoing DAS
      if((tris_state.left_first && key == settings.right) || (!tris_state.left_first && key == settings.left)) {
        tris_state.das_start = gfx_time();
        tris_state.das_dir = -tris_state.das_dir;
      }
      else tris_state.das_start = 0.0;
      tris_state.das_done = false;
    }
  }
  
  else if (pressed) {
    if(key == settings.harddrop) {
      controlled->pos.y = project_down(controlled);
      place(controlled);
    }

    else if(key == settings.down)
      controlled->pos.y = project_down(controlled);

    else if((key == settings.rotate_cw && (direction = 1)) ||
            (key == settings.rotate_ccw && (direction = -1)) ||
            (key == settings.rotate_180 && (direction = 2)))
      rotate(controlled, direction);

    else if(key == settings.hold) {
      if(!controlled->hold) {
        controlled->hold = controlled->type;
        pop_queue(controlled);
        return;
      }
      if(controlled->held_for_piece) return;
      controlled->held_for_piece = true;
      u8 tmp = controlled->type;
      controlled->type = controlled->hold;
      controlled->hold = tmp;
      controlled->pos = (gfx_vector_micro) { PIECE_START };
      controlled->rotation = 0;
    }
  }
}


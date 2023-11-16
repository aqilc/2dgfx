
/*
  Copyright ©AqilCont (aqilcm@gmail.com)
  Commons Clause Licence
  
  TODO:
  - [ ] Keep track of the order of things with z axis
  - [ ] Need to completely stop uploading the whole draw buffer every frame
  - [ ] Add rounded shapes
    - [ ] Probably separate draw call ;w;
  - [ ] Add dynamic shapes, like begin(), vertex(), bezier() or something like that
    - More shapes than quad in the first place
    - Would require new shader systems and draw calls
  - [ ] Put the main typeface in the actual like system
  - [ ] Use a sampler2d array in the shader to draw up to 32 textures at once, which would enable a lot more granularity and speed a lot of things up.
    - [ ] Would also require a bool array but it's fine we can come up with the solution later
  - [ ] Precompile shaders to SPIR-V
  - [ ] Draw 3d Shapes
  DONE(OMG WTH NICE BRO):
  - [x] Remove `loadchars`, make it simpler for user
    - [x] Init freetype in loadglgraphics()
    - [x] loadfont()
  - [x] TEXTURES AND IMAGES WHEN BRUH!??!?!?!??!?! I NEED THAT HOT ANIME GIRL BG!!!!!!
*/

#include <2dgraphics.h>
#include <futil.h>

// temporary stuff
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb/stb_image_write.h"

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb/stb_image.h>


#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define _LINE STRINGIFY(__LINE__)

typedef struct shapedata {
  vec2 pos;
  vec2 tex;
  vec4 col;
} shapedata;

typedef struct shapeheap {
  struct {
    shapedata* b;
    u16 cur;
    u16 size;
  } data;
  struct {
    u16* b;
    u16 cur;
    u16 size;
  } ib;
  u32 shapes;
  bool changed;
  bool enlarged;
} shapeheap;


static void shapeinsert(shapedata* buf, u16* ib, u16 bs, u16 is);
static void shape(shapedata* data, u16* ib, u16 bs, u16 is, vec4 col);
static void bindimage(img img);
static img imageinsert(imagedata* image);
static void useslot(u32 slot);
static GLuint freeslot();
static GLuint findslot();

struct bufib { shapedata* buf; u16* ib; };
static struct bufib quad(shapedata* buf, u16* ib, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);



// Data for the texture atlas for fonts and shapes
#define TEXTUREW 512
#define TEXTUREH 512
static u16 textsize = 48;

// Keeps track of drawn images so we don't have extreme perf penalties when drawing images
static size_t imageslen = 0;
static imagedata* images = NULL;

// Data for drawing shapes
static shapeheap* sh = NULL;
static drawcontext* ctx = NULL;
static char textures = 0;
static u32 tex = 0;

// Context color
static vec4 col = { 1.0f, 1.0f, 1.0f, 1.0f };

// Current font and font store
static hashtable* cses = NULL;
static char* font = NULL;
static float line_height = 1.2;

static FT_Library ft = {0};
static FT_Face* faces = NULL; // Array of faces so user doesn't have to take care of deallocating them later.. at least that's the idea i think
static u32 faceslen = 0;

// Scenes api, to save as many draws and buffer swaps as possible :D
// static int scene;

// Initializes everything for text, including setting up textures etc
void loadglgraphics() {
  
  // Sets the debug message callback so we can detect opengl errors
  glDebugMessageCallback(MessageCallback, NULL);

  // Enables blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);

  // Initialized VA and shaders
  initcontext(&ctx, "./graphics.glsl");

  // Sets up layout
  ctx->layout = calloc(sizeof(vlayout), 1);
  lpushf(ctx->layout, 2);
  lpushf(ctx->layout, 2);
  lpushf(ctx->layout, 4);
  
  // Allocs the heap
  sh = calloc(1, sizeof(shapeheap));
  sh->data.b = malloc(10 * sizeof(shapedata));
  sh->data.size = 10;
  sh->data.cur = 0;
  sh->ib.b = malloc(10 * sizeof(shapedata));
  sh->ib.size = 10;
  sh->ib.cur = 0;
  sh->enlarged = true;

  cses = ht(10);
}

typeface* loadfont(char* file) {
  if(!faces) faces = malloc(sizeof(FT_Face));
  else faces = realloc(faces, sizeof(FT_Face) * (faceslen + 1));
  FT_Face* face = faces + faceslen;
  faceslen++;

  // Loads the freetype library.
  if(!ft) if(FT_Init_FreeType(&ft))
    puts("Couldn't init freetype :(\nLine: "_LINE" | File: "__FILE__"\n");

  // Loads the new face in using the library
  if(FT_New_Face(ft, file, 0, face)) {
    printf("Couldn't load font '%s' :(\nLine: "_LINE" | File: "__FILE__"\n", file); return NULL; }
  FT_Set_Pixel_Sizes(*face, 0, 48);
  return loadchars(*face, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890[]{}()/\\=+\'\"<>,.-_?|!@#$%^&* :");
}

void doneloadingfonts() {
  if(!ft) return;
  for(u32 i = 0; i < faceslen; i ++)
    FT_Done_Face(faces[i]);
  free(faces);
  FT_Done_FreeType(ft);
}

// ---------------- Text things ---------------- //
void tfont(char* name) {
  typeface* cs = htg(cses, name);
  if(!cs->gl) {

    // Sets active texture to the next one in the array
    GLuint slot = findslot();
    activet(slot);

    // Gets textureid and stuff
    cs->gl = texture(cs->tex, cs->texsize.w, cs->texsize.h, GL_RED, true);
    cs->slot = slot;

    // printf("typeface put into slot %d\n", cs->gl);

    // Sets the texture sampler to the one we just made
    setui("u_tex", slot);
    textures ++;
    return;
  }

  activet(cs->slot);
  bindt(cs->gl);
  setui("u_tex", cs->slot);
}

void tsiz(u16 size) { textsize = size; }
void text(char* text, int x, int y) {
  float scale = (float) textsize / 48.0f;
  
  // Sets up context
  // setcontext(ctx);

  // The typeface we're using to get the characters
  typeface* cs = NULL;

  // If there's no fonts loaded into the font store, don't draw anything
  if(!cses->count) return;
  else if(!font) {
    font = ((typeface*) htg(cses, cses->keys[0]))->name;
    // tfont(font);
    cs = htg(cses, font);
  } else cs = htg(cses, font);
  
  // Sets up buffers
  u32 len = strlen(text);
  shapedata* vb = malloc(4 * len * sizeof(shapedata)); // 4 vertices per letter for the rectangle that the letter has
  u16* ib = malloc(6 * sizeof(u16) * len); // 6 vertices(2 triangles) per char
  
  // Generates every vertex
  u16 cur = sh->data.cur;
  GLuint slot = cs->slot;
  float xp = 0, yp = 0, w = 0, h = 0, tx = 0, ty = 0, th = 0, tw = 0;
  int initial_x = x;
  u32 i = 0;
  char* tmpchar = new_c(0);
  for(; *text; text++) {

    // Newlines in five lines :D
    if (*text == '\n') {
      y += (int) (line_height * textsize);
      x = initial_x;
      continue;
    }

    else if(*text == ' ') {
      x += cs->space_width * scale;
      continue;
    }

    // Gets the Char object from the hashmap for the character lib
    tmpchar[0] = *text;
    Char* c = htg(cs->chars, tmpchar);
    
    if(!c) { printf("bruh wtf u drawin \"%c\"(%X)\t", *text, *text); continue; }
    
    xp = (float) x + c->bearing.x * scale;
    yp = (float) y - c->bearing.y * scale;
    w = (float) c->size.w * scale;
    h = (float) c->size.h * scale;

    tx = c->place.x / (float) cs->texsize.w;
    ty = c->place.y / (float) cs->texsize.h;
    tw = c->size.w / (float) cs->texsize.w;
    th = c->size.h / (float) cs->texsize.h;
    
    vb[i * 4] =     (shapedata) {{ xp, yp }, { tx, ty }, { col[0], col[1], col[2], col[3] }};
    vb[i * 4 + 1] = (shapedata) {{ xp + w, yp }, { tx + tw, ty }, { col[0], col[1], col[2], col[3] }};
    vb[i * 4 + 2] = (shapedata) {{ xp, yp + h }, { tx, ty + th }, { col[0], col[1], col[2], col[3] }};
    vb[i * 4 + 3] = (shapedata) {{ xp + w, yp + h }, { tx + tw, ty + th }, { col[0], col[1], col[2], col[3] }};

    // Advance cursors for next glyph
    x += (float) c->advance * scale;
    i++;
  }

  u32 written = i;
  
  // Sets all of the indexes of the index buffer, pretty straightforward
  for(i = 0; i < written; i ++) {
    //  +-----------+
    //  |0         1|
    //  ||^\        |
    //  ||   \      |
    //  ||     \    |
    //  ||       \  |
    //  |2-------->3|
    //  +-----------+
    ib[i * 6]     = i * 4 + 3 + cur;
    ib[i * 6 + 1] = i * 4 + 0 + cur;
    ib[i * 6 + 2] = i * 4 + 2 + cur;
    
    //  +-----------+
    //  |0-------->1|
    //  | ^\       ||
    //  |    \     ||
    //  |      \   ||
    //  |        \ ||
    //  |2         3|
    //  +-----------+
    ib[i * 6 + 3] = i * 4 + 1 + cur;
    ib[i * 6 + 4] = i * 4 + 3 + cur;
    ib[i * 6 + 5] = i * 4 + 0 + cur;
  }

  shapeinsert(vb, ib, 4 * written, 6 * written);
  free(tmpchar);
  free(vb);
  free(ib);
}

void draw() {
  setui("u_tex", 0);
  setui("u_shape", true);
  binda(ctx->vaid);
  if(!ctx->vbid) {
    ctx->vbid = create_vb(sh->data.b, sh->data.cur * sizeof(shapedata));
    ctx->ibid = create_ib(sh->ib.b, sh->ib.cur);
    lapply(ctx->layout);
  } else {
    // bindv(ctx->vbid);
    glBufferData(GL_ARRAY_BUFFER, sh->data.cur * sizeof(shapedata), sh->data.b, GL_DYNAMIC_DRAW);
    // bindi(ctx->ibid);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sh->ib.cur * sizeof(u16), sh->ib.b, GL_DYNAMIC_DRAW);
    // sh->enlarged = false;
  }
  // } else if(sh->changed && sh->data.cur) {
  //   int vao, vbo, ibo, siz, siz2;
  //   glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  //   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
  //   glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ibo);
  //   glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &siz);
  //   glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &siz2);
  //   printf("vao: %d, vbo: %d, ibo: %d, array buffer size: %d, elem array buffer size: %d, arraysize should be: %d\n", vao, vbo, ibo, siz, siz2, sh->data.cur * sizeof(shapedata));
  //   // bindv(ctx->vbid);
  //   glBufferSubData(GL_ARRAY_BUFFER, 0, sh->data.cur * sizeof(shapedata), sh->data.b);
  //   // bindi(ctx->ibid);
  //   glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sh->ib.cur * sizeof(u16), sh->ib.b);
  // }

  if(sh->data.cur)
    glDrawElements(GL_TRIANGLES, sh->ib.cur, GL_UNSIGNED_SHORT, NULL);//sh->ib.b);

  // Resets cursors and since the data has been processed, reset the changed and enlarged booleans
  sh->data.cur = 0;
  sh->ib.cur = 0;
  sh->changed = false;
  sh->enlarged = false;
}

typeface* loadchars(FT_Face face, char* chars) {
  
  // Creates a new typeface
  typeface* pog = calloc(sizeof(typeface), 1);
  pog->chars = ht(200);
  union vec texsize = { TEXTUREW, TEXTUREH };
  pog->texsize = texsize;
  pog->tex = malloc(pog->texsize.x * pog->texsize.y * sizeof(u8));
  pog->name = new_s(face->family_name);

  // Root node for the texture fitting algo
  CharNode root = {.s = texsize};

  int len = strlen(chars);
  for(int i = 0; i < len; i ++) {
    FT_Error error = FT_Load_Char(face, chars[i], FT_LOAD_RENDER);
    if(error) printf("Failed to load glyph '%c' (error code: %d | Line: "_LINE" | File: "__FILE__")\n", chars[i], error);

    // Skip spaces, since you don't really need to draw anything
    if(chars[i] == ' ') {
      pog->space_width = face->glyph->advance.x >> 6; continue; }
    
    int width = face->glyph->bitmap.width;
    int height = face->glyph->bitmap.rows;
    union vec size = { width, height };
    
    CharNode* rec = charnode_insert(&root, &size);
    if(!rec) { printf("Could not fit char %c into the texture map\n", chars[i]); break; }
    
    // Shorthands
    int x = rec->p.x;
    int y = rec->p.y;
    int w = rec->s.w;
    int h = rec->s.h;
    
    // Defines the metrics for the glyph
    Char* ch = malloc(sizeof(Char));
    ch->c = chars[i];
    ch->size = size;
    ch->bearing = (union vec) { face->glyph->bitmap_left, face->glyph->bitmap_top };
    ch->advance = face->glyph->advance.x >> 6; // bitshift by 6 to get value in pixels (2^6 = 64)

    // Remembers the place of the glyph in the atlas
    ch->place.x = x;
    ch->place.y = y;
    
    // big boi for loop that writes the bitmap buffer from the freetype glyph into the font atlas
    for(int i = 0; i < w * h; i ++) pog->tex[x + y * texsize.w + i % w + (i / w * texsize.h)] = face->glyph->bitmap.buffer[i];
    
    // Inserts it into the map so we can look it up later
    hti(pog->chars, new_c(chars[i]), ch);
  }

  // Frees up a crap ton of memory
  charnode_free(root.child[0]);
  charnode_free(root.child[1]);

  // Draws a little white square in the corner of the texture atlas for shapes
  for(int i = 16; i > 0; i --) pog->tex[texsize.w * texsize.h - i % 4 - (i / 4 * texsize.h)] = 255;

  // So we can ~~admire~~inspect the generated font atlas later
  // stbi_write_png("bitmap.png", texsize.w, texsize.w, 1, pog->tex, texsize.w);

  // Sets it as the current font
  hti(cses, new_s(pog->name), pog);
  printf("Font \"%s\" loaded\n", pog->name);
  tfont(pog->name);

  return pog;
}

static struct bufib quad(shapedata* buf, u16* ib, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {

  // Creates the shape
  // shapedata* buf = malloc(sizeof(shapedata) * 4);
  buf[0].pos[0] = (float) x1;
  buf[1].pos[0] = (float) x2;
  buf[2].pos[0] = (float) x3;
  buf[3].pos[0] = (float) x4;
  buf[0].pos[1] = (float) y1;
  buf[1].pos[1] = (float) y2;
  buf[2].pos[1] = (float) y3;
  buf[3].pos[1] = (float) y4;

  // Creates the index buffer
  // u16* ib = malloc(sizeof(u16) * 6);
  ib[0] = 0;
  ib[1] = 1;
  ib[2] = 2;
  ib[3] = 1;
  ib[4] = 2;
  ib[5] = 3;

  return (struct bufib) { buf, ib };
}


// Create a temporary array of texture coords so we can properly set them later
static float bruhwhyc[] = { 1.0f - 2.5f/ (float)TEXTUREW, 1.0f - 2.5f/ (float) TEXTUREH, 1.0f - 2.5f/ (float) TEXTUREW, 1.0f, 1.0f, 1.0f - 2.5f/ (float) TEXTUREH, 1.0f, 1.0f };
static void shape(shapedata* data, u16* ib, u16 bs, u16 is, vec4 col) {
  int cur = sh->data.cur;
  for(int i = 0; i < is; i ++) ib[i] += cur;

  // Sets up color and texture coords for all 4 verts at once
  for(int i = 0; i < bs; i ++)
    memcpy(&data[i].col, col, sizeof(vec4)), memcpy(&data[i].tex, bruhwhyc + (i % 4) * 2, sizeof(vec2));
}

void shapecolor(vec4 col, u16 verts) {
  for(int i = verts; i > 0; i --)
    memcpy(sh->data.b[sh->data.cur - i].col, col, sizeof(vec4));
}

void rect(int x, int y, int w, int h) {
  u16 ib[6]; shapedata verts[4];
  struct bufib hi = quad(verts, ib, x, y, x + w, y, x, y + h, x + w, y + h);
  shape(hi.buf, hi.ib, 4, 6, col);
  shapeinsert(hi.buf, hi.ib, 4, 6);
}


void fill(float c1, float c2, float c3, float c4) {
  col[0] = c1; col[1] = c2; col[2] = c3; col[3] = c4; }

void skiprec(u16 n) {
  sh->data.cur += n * 4;
  sh->ib.cur += n * 6;
}
static void shapeinsert(shapedata* buf, u16* ib, u16 bs, u16 is) {

  // If the memory buffer for the buffer data heap is not enough,
  if(sh->data.size < sh->data.cur + bs){

    // Ask for bigger size
    sh->data.b = realloc(sh->data.b, sizeof(shapedata) * (sh->data.cur + bs));

    // Copy into it
    memcpy(sh->data.b + sh->data.cur, buf, bs * sizeof(shapedata));

    // Advance cursor and size
    sh->data.cur += bs;
    sh->data.size = sh->data.cur;

    // Set changed and enlarged so we can reupload the data later
    sh->changed = true;
    sh->enlarged = true;
    // printf("Enlarged buffer to %d\n", sh->data.size);
  }
  // If it is enough, but the memory isn't the same
  else// if (memcmp(sh->data.b + sh->data.cur, buf, bs))
  {
    // Copy the memory in and just change it
    memcpy(sh->data.b + sh->data.cur, buf, bs * sizeof(shapedata));
    sh->data.cur += bs;
    sh->changed = true;
  }// else sh->data.cur += bs;

  if(sh->ib.size < sh->ib.cur + is){
    sh->ib.b = realloc(sh->ib.b, sizeof(u16) * (sh->ib.cur + is));
    memcpy(sh->ib.b + sh->ib.cur, ib, is * sizeof(u16));
    sh->ib.cur += is;
    sh->ib.size = sh->ib.cur;
    sh->changed = true;
    sh->enlarged = true;
  }
  else// if (memcmp(sh->ib.b + sh->ib.cur, buf, is))
  {
    memcpy(sh->ib.b + sh->ib.cur, ib, is * sizeof(u16));
    sh->changed = true;
    sh->ib.cur += is;
  }// else sh->ib.cur += is;
}


// ---------------- Image things ---------------- //

img loadimage(char* path) {
  if(!exists(path)) { printf("filename didn't exist: %s", path); return 0; }
  u32 x, y;
  u8* data = read_image(path, &x, &y);
  if(!data) puts("Failed to load image");
  printf("Loaded image '%s' (%d, %d)\n", path, x, y);

  GLenum slot = findslot();
  // printf("Used slot %d\n", slot);
  activet(slot);
  GLuint t = texture(data, x, y, GL_RGBA, false);

  u32 hi = hash(path, strlen(path));
  imagedata img = {
    .id = hi, .type = GL_RGBA, .slot = slot,
    .size = (union vec) { .w = x, .h = y },
    .uses = 0, .typeface = false, .name = new_s(path)
  };
  
  free(data);
  return imageinsert(&img);
}

img loadpixelart(char* path) {
  if(!exists(path)) { printf("filename didn't exist: %s", path); return 0; }
  u32 x, y;
  u8* data = read_image(path, &x, &y);
  if(!data) puts("Failed to load image");
  printf("Loaded image '%s' (%d, %d)\n", path, x, y);

  GLenum slot = findslot();
  // printf("Used slot %d\n", slot);
  activet(slot);
  GLuint t = pixpertex(data, x, y, GL_RGBA);

  u32 hi = hash(path, strlen(path));
  imagedata img = {
    .id = hi, .type = GL_RGBA, .slot = slot,
    .size = (union vec) { .w = x, .h = y },
    .uses = 0, .typeface = false, .name = new_s(path)
  };
  
  free(data);
  return imageinsert(&img);
}

static inline imagedata* simgd(img index) {
  if(index >= imageslen) return NULL;
  return images + index;
}

imagedata* imgd(img index) { return simgd(index); }


static u32 usedslots = 0; // USE BITWISE OPS TO KEEP TRACK OF THE SLOTS OMG WTH SO BIG BRAIN
static void useslot(u32 slot) {
  if (slot > 31) return;
  u32 s = (u32) 1 << slot;
  if(usedslots & (u32) 1 << slot) { usedslots -= s; return; }
  usedslots += s;
}

// https://stackoverflow.com/questions/21623614/finding-the-first-set-bit-in-a-binary-number
static GLuint freeslot() {
  for(GLuint i = 0; i < 32; i ++)
    if(~(usedslots >> i) & 1) return i;
  return 4294967295; // u32 max for fun :D
}

static GLuint findslot() {
  GLuint slot = freeslot();
  if(slot < 32) { useslot(slot); return slot; }
  for(int i = 0; i < imageslen; i ++)
    if(images[i].typeface || images[i].slot > 32) continue;
    else if (images[i].uses == 0) return images[i].slot;
  return 31;
}

static bool printed = false;
static void bindimage(img index) {
  imagedata* img = simgd(index);
  if(img->slot > 31) { if(!printed) { printed = true; printf("Invalid slot %d for image with path: %s\n", img->slot, img->name); } return; }
  setui("u_tex", img->slot);
  setui("u_shape", false);
}

void image(img image, int ix, int iy, int iw, int ih) {
  if(!ctx->vbid) return;
  bindimage(image);
  // binda(ctx->vaid);
  // bindv(ctx->vbid);
  float x = ix; float y = iy; float w = iw; float h = ih;
  shapedata tl[6] = {
      {{x, y}, {0.f, 0.f}, {col[0], col[1], col[2], col[3]}},
      {{x, y + h}, {0.f, 1.f}, {col[0], col[1], col[2], col[3]}},
      {{x + w, y}, {1.f, 0.f}, {col[0], col[1], col[2], col[3]}},
      {{x + w, y}, {1.f, 0.f}, {col[0], col[1], col[2], col[3]}},
      {{x, y + h}, {0.f, 1.f}, {col[0], col[1], col[2], col[3]}},
      {{x + w, y + h}, {1.f, 1.f}, {col[0], col[1], col[2], col[3]}},
  };
  if (!sh->data.cur)
    glBufferData(GL_ARRAY_BUFFER, sizeof(tl), tl, GL_DYNAMIC_DRAW);
  else glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tl), tl);

  glDrawArrays(GL_TRIANGLES, 0, 6);
}


/*
 * Displays a part of an image, useful for displaying sprites.
 *
 * @param ind - Index of image inserted to storage.
*/
void imagesub(img ind, int ix, int iy, int iw, int ih, int itx, int ity, int itw, int ith) {
  if(!ctx->vbid) return;
  bindimage(ind);
  imagedata* image = simgd(ind);
  // binda(ctx->vaid);
  // bindv(ctx->vbid);
  float x = ix; float y = iy; float w = iw; float h = ih;
  float otw = image->size.w; float oth = image->size.h;
  float tx = (float)itx / otw; float ty = (float) ity / oth; float tw = (float)itw / otw; float th = (float)ith / oth;
  shapedata tl[6] = {
    {{  x, y  }, {   tx, ty   }, {col[0], col[1], col[2], col[3]}},
    {{  x, y+h}, {   tx, ty+th}, {col[0], col[1], col[2], col[3]}},
    {{x+w, y  }, {tw+tx, ty   }, {col[0], col[1], col[2], col[3]}},
    {{x+w, y  }, {tw+tx, ty   }, {col[0], col[1], col[2], col[3]}},
    {{  x, y+h}, {   tx, ty+th}, {col[0], col[1], col[2], col[3]}},
    {{x+w, y+h}, {tx+tw, ty+th}, {col[0], col[1], col[2], col[3]}},
  };
  if(!sh->data.cur)
    glBufferData(GL_ARRAY_BUFFER, sizeof(tl), tl, GL_DYNAMIC_DRAW);
  else glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tl), tl);
  
  glDrawArrays(GL_TRIANGLES, 0, 6);
}


spritesheet* createss(img img, int step_x, int step_y) {
  spritesheet* s = malloc(sizeof(spritesheet));
  s->img = img;
  s->step_y = step_y;
  s->step_x = step_x;
  return s;
}

void sprite(spritesheet* s, int sx, int sy, int x, int y, int w, int h) {
  imagesub(s->img, x, y, w, h, sx * s->step_x, sy * s->step_y, s->step_x, s->step_y);
}


static img imageinsert(imagedata* image) {
  if(!images) images = malloc(sizeof(imagedata));
  else images = realloc(images, (imageslen + 1) * sizeof(imagedata));
  images[imageslen] = *image;
  imageslen ++;
  return imageslen - 1;
}



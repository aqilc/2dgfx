#pragma once

#include <stdint.h>
#include <stdbool.h>


typedef union gfx_vector_big {
  struct { int64_t x, y; };
  struct { double dx, dy; };
  struct { uint64_t w, h; };
} gfx_vector_big;

typedef union gfx_vector {
  struct { int x, y; };
  struct { float fx, fy; };
  struct { uint32_t w, h; };
  int64_t full;
} gfx_vector;

typedef union gfx_vector_mini {
  struct { int16_t x, y; };
  struct { uint16_t w, h; };
  int full;
} gfx_vector_mini;

typedef union gfx_vector_micro {
  struct { int8_t x, y; };
  struct { uint8_t w, h; };
  int16_t full;
} gfx_vector_micro;

enum gfx_setting_name {
  GFX_SETTING_WIDTH,
  GFX_SETTING_HEIGHT,
  GFX_SETTING_APP_NAME,
  GFX_SETTING_VSYNC,
  GFX_SETTING_TRANSPARENT,
  GFX_SETTING_NO_RESIZE,
  GFX_SETTING_NO_DECORATIONS,
  GFX_SETTING_DONT_STORE_SETTINGS,
  GFX_SETTING_MSAA,
  GFX_SETTING_INITIAL_WINDOW
};
typedef struct gfx_settings {
  uint32_t width, height;
  const char* app_name;
  bool vsync;
  bool transparent;
  bool no_resize;
  bool no_decorations;
  bool dont_store_settings;
  float fps_recalc_delta;
  uint8_t msaa;
  enum gfx_setting_initial_window_mode: uint8_t {
    GFX_WIN_DEFAULT,
    GFX_WIN_FULLSCREEN,
    GFX_WIN_MAXIMIZED,
    GFX_WIN_MINIMIZED
  } initial_window;
} gfx_settings;

typedef enum gfx_store_type {
  GFX_TYPE_INT, GFX_TYPE_DOUBLE, GFX_TYPE_STRING, GFX_TYPE_INT64, GFX_TYPE_BINARY
} gfx_store_type;

enum gfx_ids {
  img_null = -1,
};

typedef enum gfx_keycode {
  GFX_KEY_SPACE         = 32,
  GFX_KEY_APOSTROPHE    = 39, /* ' */
  GFX_KEY_COMMA         = 44, /* , */
  GFX_KEY_MINUS         = 45, /* - */
  GFX_KEY_PERIOD        = 46, /* . */
  GFX_KEY_SLASH         = 47, /* / */
  GFX_KEY_0 = 48, GFX_KEY_1 = 49, GFX_KEY_2 = 50, GFX_KEY_3 = 51, GFX_KEY_4 = 52, GFX_KEY_5 = 53, GFX_KEY_6 = 54, GFX_KEY_7 = 55, GFX_KEY_8 = 56, GFX_KEY_9 = 57,
  GFX_KEY_SEMICOLON     = 59, /* ; */
  GFX_KEY_EQUAL         = 61, /* = */
  GFX_KEY_A = 65, GFX_KEY_B = 66, GFX_KEY_C = 67, GFX_KEY_D = 68, GFX_KEY_E = 69, GFX_KEY_F = 70, GFX_KEY_G = 71, GFX_KEY_H = 72, GFX_KEY_I = 73,
  GFX_KEY_J = 74, GFX_KEY_K = 75, GFX_KEY_L = 76, GFX_KEY_M = 77, GFX_KEY_N = 78, GFX_KEY_O = 79, GFX_KEY_P = 80, GFX_KEY_Q = 81, GFX_KEY_R = 82,
  GFX_KEY_S = 83, GFX_KEY_T = 84, GFX_KEY_U = 85, GFX_KEY_V = 86, GFX_KEY_W = 87, GFX_KEY_X = 88, GFX_KEY_Y = 89, GFX_KEY_Z = 90,
  GFX_KEY_LEFT_BRACKET  = 91, /* [ */
  GFX_KEY_BACKSLASH     = 92, /* \ */
  GFX_KEY_RIGHT_BRACKET = 93, /* ] */
  GFX_KEY_GRAVE_ACCENT  = 96, /* ` */
  GFX_KEY_WORLD_1       = 161, /* non-US #1 */
  GFX_KEY_WORLD_2       = 162, /* non-US #2 */
  GFX_KEY_ESCAPE        = 256,
  GFX_KEY_ENTER         = 257,
  GFX_KEY_TAB           = 258,
  GFX_KEY_BACKSPACE     = 259,
  GFX_KEY_INSERT        = 260,
  GFX_KEY_DELETE        = 261,
  GFX_KEY_RIGHT         = 262,
  GFX_KEY_LEFT          = 263,
  GFX_KEY_DOWN          = 264,
  GFX_KEY_UP            = 265,
  GFX_KEY_PAGE_UP       = 266,
  GFX_KEY_PAGE_DOWN     = 267,
  GFX_KEY_HOME          = 268,
  GFX_KEY_END           = 269,
  GFX_KEY_CAPS_LOCK     = 280,
  GFX_KEY_SCROLL_LOCK   = 281,
  GFX_KEY_NUM_LOCK      = 282,
  GFX_KEY_PRINT_SCREEN  = 283,
  GFX_KEY_PAUSE         = 284,
  GFX_KEY_F1 = 290, GFX_KEY_F2 = 291, GFX_KEY_F3 = 292, GFX_KEY_F4 = 293, GFX_KEY_F5 = 294, GFX_KEY_F6 = 295, GFX_KEY_F7 = 296,
  GFX_KEY_F8 = 297, GFX_KEY_F9 = 298, GFX_KEY_F10 = 299, GFX_KEY_F11 = 300, GFX_KEY_F12 = 301, GFX_KEY_F13 = 302, GFX_KEY_F14 = 303,
  GFX_KEY_F15 = 304, GFX_KEY_F16 = 305, GFX_KEY_F17 = 306, GFX_KEY_F18 = 307, GFX_KEY_F19 = 308, GFX_KEY_F20 = 309, GFX_KEY_F21 = 310,
  GFX_KEY_F22 = 311, GFX_KEY_F23 = 312, GFX_KEY_F24 = 313, GFX_KEY_F25 = 314,
  GFX_KEY_KP_0          = 320,
  GFX_KEY_KP_1          = 321,
  GFX_KEY_KP_2          = 322,
  GFX_KEY_KP_3          = 323,
  GFX_KEY_KP_4          = 324,
  GFX_KEY_KP_5          = 325,
  GFX_KEY_KP_6          = 326,
  GFX_KEY_KP_7          = 327,
  GFX_KEY_KP_8          = 328,
  GFX_KEY_KP_9          = 329,
  GFX_KEY_KP_DECIMAL    = 330,
  GFX_KEY_KP_DIVIDE     = 331,
  GFX_KEY_KP_MULTIPLY   = 332,
  GFX_KEY_KP_SUBTRACT   = 333,
  GFX_KEY_KP_ADD        = 334,
  GFX_KEY_KP_ENTER      = 335,
  GFX_KEY_KP_EQUAL      = 336,
  GFX_KEY_LEFT_SHIFT    = 340,
  GFX_KEY_LEFT_CONTROL  = 341,
  GFX_KEY_LEFT_ALT      = 342,
  GFX_KEY_LEFT_SUPER    = 343,
  GFX_KEY_RIGHT_SHIFT   = 344,
  GFX_KEY_RIGHT_CONTROL = 345,
  GFX_KEY_RIGHT_ALT     = 346,
  GFX_KEY_RIGHT_SUPER   = 347,
  GFX_KEY_MENU          = 348,
} gfx_keycode;

typedef enum gfx_keymod {
  GFX_MOD_SHIFT     = 0x0001,
  GFX_MOD_CONTROL   = 0x0002,
  GFX_MOD_ALT       = 0x0004,
  GFX_MOD_SUPER     = 0x0008,
  GFX_MOD_CAPS_LOCK = 0x0010,
  GFX_MOD_NUM_LOCK  = 0x0020,
} gfx_keymod;

typedef enum gfx_mouse_button {
  GFX_MOUSE_LEFT, GFX_MOUSE_RIGHT, GFX_MOUSE_MIDDLE, GFX_MOUSE_X1,
  GFX_MOUSE_X2, GFX_MOUSE_X3, GFX_MOUSE_X4, GFX_MOUSE_X5, GFX_MOUSE_X6
} gfx_mouse_button;

struct gfx_ctx;

typedef int gfx_img;
typedef int gfx_face;

// Initializes a 2DGFX Context and sets up OpenGL, heaps and buffers.
struct gfx_ctx* gfx_init(const char* title, gfx_settings* settings);
bool gfx_frame();
void gfx_quit();

// When using multiple 2DGFX contexts, switch contexts with this.
void gfx_ctx_set(struct gfx_ctx* c);

// Loads a font through FreeType
gfx_face gfx_load_font(const char* file);

// Loads in image through STB_Image
gfx_img gfx_load_img(const char* file);
gfx_img gfx_load_img_mem(uint8_t* t, uint32_t len);
gfx_img gfx_load_img_rgba(uint8_t* t, int width, int height);

// Input functions
gfx_vector gfx_mouse();
gfx_vector gfx_screen_dims();

// Drawing commands
void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a); // Specify color for the next set of shapes.
void quad(short x1, short y1, short x2, short y2, short x3, short y3, short x4, short y4);
void rect(short x, short y, short w, short h);
// void ellipse(short x, short y, short rx, short ry);
// void ellipse_s(short x, short y, short rx, short ry, short stroke);
// void circle(short x, short y, short r); // Circle!!

// Image drawing commands
void image(gfx_img img, short x, short y, short w, short h); // Draws an image at those coordinates.
gfx_vector_mini* const isize(gfx_img img); // Image size.

// Text Drawing commands
void font_size(uint32_t size);
void lineheight(float h);
void font(gfx_face face, uint32_t size);
void text(const char* str, short x, short y);
void textf(short x, short y, const char* fmt, ...); // SLOW, AVOID UNLESS DEBUGGING

// Calculates FPS
double gfx_time();
void gfx_sleep(uint32_t miliseconds);
double gfx_fps();
bool gfx_fps_changed();
void gfx_default_fps_counter();

void on_mouse_button(gfx_vector pos, gfx_mouse_button button, bool pressed, gfx_keymod mods);
void on_mouse_move(gfx_vector pos);
void on_mouse_enter(bool entered);
void on_mouse_scroll(gfx_vector scroll);
void on_key_button(gfx_keycode key, bool pressed, gfx_keymod mods);
void on_key_char(uint32_t utf_codepoint);
void on_window_resize(gfx_vector size);
void on_window_move(gfx_vector pos);

char const* const gfx_key_name(gfx_keycode key);

#ifdef GFX_MAIN_DEFINE

  // https://stackoverflow.com/a/53388334/10013227
# if defined(_MSC_VER) && !defined(__clang__)
    void setup(void);
    void loop(void);
    void cleanup(void);
    
#   pragma comment(linker, " /alternatename:setup=setup__ /alternatename:loop=setup__ /alternatename:cleanup=setup__")
# else
    void setup(void) __attribute__((weak));
    void loop(void) __attribute__((weak));
    void cleanup(void) __attribute__((weak));
# endif

  int main() {
    if(setup) setup();
    if(loop) while(gfx_frame()) loop();
    if(cleanup) cleanup();
    return 0;
  }
#endif


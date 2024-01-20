#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef union gfx_vector {
  struct { int x, y; };
  struct { float fx, fy; };
  struct { uint32_t w, h; };
} gfx_vector;

struct gfx_ctx;

typedef int img;
typedef int typeface;

// Initializes a 2DGFX Context and sets up OpenGL, heaps and buffers.
struct gfx_ctx* gfx_init(const char* title, uint32_t w, uint32_t h);
bool gfx_nextframe();
void gfx_frameend();
void gfx_close();

// When using multiple 2DGFX contexts, switch contexts with this.
void gfx_curctx(struct gfx_ctx* c);

// Loads a font through FreeType
typeface gfx_loadfont(const char* file);

// Loads in image through STB_Image
img gfx_loadimg(char* file);

// Input functions
gfx_vector gfx_mouse();
gfx_vector gfx_screen_dims();

// Drawing commands
void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a); // Specify color for the next set of shapes.
void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4); // Quad
void rect(int x, int y, int w, int h); // Plain Rectangle
void circle(int x, int y, int r); // Circle!!

// Image drawing commands
void image(img img, int x, int y, int w, int h); // Draws an image at those coordinates.
gfx_vector* isz(img img); // Image size.

// Text Drawing commands
void fontsize(uint32_t size);
void lineheight(float h);
void font(typeface face, uint32_t size);
void text(const char* str, int x, int y);

// Calculates FPS
double gfx_fps();
bool gfx_fpschanged();

#ifdef GFX_MAIN_DEFINE
void loop();
void setup();

int main() {
  setup();
  while(gfx_nextframe()) {
    loop();
    gfx_frameend();
  }
  return 0;
}
#endif


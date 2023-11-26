
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdint.h>
#include <stdbool.h>

typedef union gfx_vector {
  struct { int x, y; };
  struct { float fx, fy; };
  struct { int w, h; };
} gfx_vector;

struct gfx_ctx;

typedef int img;
typedef uint32_t typeface;

// Initializes a 2DGFX Context and sets up OpenGL, heaps and buffers.
struct gfx_ctx* gfx_init(const char* title, uint32_t w, uint32_t h);
bool gfx_nextframe();
void gfx_frameend();

// When using multiple 2DGFX contexts, switch contexts with this.
void gfx_curctx(struct gfx_ctx* c);

// Loads a font through FreeType
typeface gfx_loadfont(const char* file);

// Loads in image through STB_Image
img gfx_loadimg(char* file);


// Input functions
gfx_vector gfx_mouse();


// Drawing commands
void fill(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
void rect(int x, int y, int w, int h);

// Image drawing commands
void image(img img, int x, int y, int w, int h);
gfx_vector* isize(img img);

// Text Drawing commands
void fontsize(uint32_t size);
void lineheight(float h);
void font(typeface face, uint32_t size);
void text(const char* str, int x, int y);

// Calculates FPS
double gfx_fps();
bool gfx_fpschanged();



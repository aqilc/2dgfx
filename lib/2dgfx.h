
#define GLEW_STATIC
#include <GL/glew.h>

#include <stdint.h>
#include <vec.h>
#include <hash.h>

typedef union gfx_vector {
  struct {
    int x, y;
  };
  struct {
    float fx, fy;
  };
  struct {
    int w, h;
  };
} gfx_vector;

typedef struct gfx_char {
  gfx_vector place;   // Place in texture atlas
  gfx_vector size;    // w, h of char texture
  gfx_vector bearing; // Refer to freetype docs on this, but I need to keep in track of it.
  uint32_t advance;   // How far this character moves forward the text cursor
  unsigned long c;    // The character(unicode)
} gfx_char;

typedef enum gfx_texture_type {
	GL_TEX_IMAGE, GFX_TEX_TYPEFACE, GFX_TEX_PIXELART
} gfx_texture_type;

typedef struct gfx_texture {
  uint8_t* tex;    // Pointer to texture's data buffer (RGBA)
  gfx_vector size;
  GLuint id;
  GLuint slot;
  uint32_t uses;   // Uses per frame
  gfx_texture_type type;
} gfx_texture;

struct gfx_atlas_node {
  gfx_vector p; // Place
  gfx_vector s; // Size
  struct gfx_atlas_node* child[2];
  bool filled;
};

typedef struct typeface {
  char* name;
  uint32_t space_width;
  gfx_texture* tex;                  // Texture atlas
  struct gfx_atlas_node* atlas_tree; // Vec<gfx_atlas_node>
  void* face;                        // FT_Face internally
  ht(unsigned long, gfx_char) chars;
} typeface;

struct gfx_ctx {
  float curcol[4];        // Current color
  uint32_t slots;         // Array of length 32 of booleans for slots(OpenGL only supports 32).
  gfx_texture* textures;  // Vec<gfx_texture>
  
  // OpenGL related variables.
  struct {
    GLuint progid, varrid, vbufid, idxbufid;
    ht(char*, GLint) uniforms;
  } gl;

  struct {
    ht(char*, typeface) store;
    typeface* cur;
    uint32_t size;
  } fonts;

  struct {
    struct gfx_drawbuf {
      gfx_vector p;
      gfx_vector tp;
      float col[4];
    }* svec;
    uint32_t* idvec;
  } drawbuf;

  struct {
    float data[4][4];
    bool changed;
    bool used;        // Calls draw if there were shapes drawn between transforms
  } transform;
};

typedef gfx_texture* img;

// Initializes a 2DGFX Context and sets up OpenGL, heaps and buffers.
struct gfx_ctx* gfx_init(uint32_t w, uint32_t h);

// When using multiple 2DGFX contexts, switch contexts with this.
void gfx_curctx(struct gfx_ctx* c);

// Loads a font through FreeType
typeface* gfx_loadfont(char* file);

// Loads in image through STB_Image
img gfx_loadimg(char* file);



// Drawing commands
void fill(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
void rect(int x, int y, int w, int h);
void image(img img, int x, int y, int w, int h);

// Draws and flushes the shape batch
void draw();





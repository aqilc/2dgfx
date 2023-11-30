/**
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/getting-started-with-windbg#summary-of-commands
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/commands
 * TODO:
 *   - Images
 *     - [-] findslot()
 *       - [ ] Return Least used image when there are no free slots.
 * 	- Text Rendering
 * 		- [x] fontsize()
 * 		- [x] font()
 * 		- [-] text()
 *       - [ ] Fix bugs
 *     - [ ] Glyph based loading and rendering.
 *     - [ ] SDF Based font renderer.
 *     - https://github.com/mightycow/Sluggish/blob/master/code/renderer_gl/main.cpp
 *     - https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
 * 	- Masking
 * 		- Determine API for drawing.
 *     - Dynamically turn on stencil testing
 * 		- https://community.khronos.org/t/texture-environment/39767/4
 */

/*
	Lessons Learned:
		- Call glGenerateMipmaps AFTER uploading texture.
		- Empty slot has to be active and 
*/

#include "2dgfx.h"

#include <stdint.h>
#include <stdio.h>

#include <vec.h>
#include <hash.h>
#include <linmath.h>

// GLEW, for opengl functions
#define GLEW_STATIC
#include <GL/glew.h>

// Window management
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define GFX_STR(...) #__VA_ARGS__
#define GFX_STR_MACRO(...) GFX_STR(__VA_ARGS__)

#ifndef GFX_FONT_ATLAS_START_SIZE
	#define GFX_FONT_ATLAS_START_SIZE 256
#endif

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

#define GFX_DEBUG
#ifdef GFX_DEBUG
	#include <stdarg.h>
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include <stb/stb_image_write.h>
	
	static inline void error_(const char* line, const char* file, const char* err, ...) {
		va_list args;
		va_start(args, err);
		vprintf(err, args);
		printf(" (Line: %s | File: %s)\n", line, file);
		va_end(args);
	}

	#define error(...) error_(GFX_STR_MACRO(__LINE__), __FILE__, "2DGFX Internal Error: " __VA_ARGS__)
	#define info(...)  error_(GFX_STR_MACRO(__LINE__), __FILE__, "           2DGFX Log: " __VA_ARGS__)

	#define CHECK_CALL(call, errorfn, ...) if(call) { error(__VA_ARGS__); errorfn; }
	
	void GLAPIENTRY glDebugMessageHandler(GLenum source,
			GLenum type, GLuint id, GLenum severity, GLsizei length,
			const GLchar* message, const void* userParam) {
		printf("        OpenGL Error: %s (Code: %d)\n", message, type);
	}

	void glfwErrorHandler(int error, const char* desc) {
		printf("GLFW Error: %s", desc);
	}
#else
	#define error(...) 
	#define info(...) 
	#define CHECK_CALL(call, errorfn, ...) if(call) { errorfn; }
	#define stbi_write_png(...) 
#endif


// -------------------------------------- Struct definitions --------------------------------------

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
  int child[2];
  bool filled;
};

typedef struct gfx_typeface {
  char* name;
  uint32_t space_width;
  uint32_t tex;                      // Texture atlas
  struct gfx_atlas_node* atlas_tree; // Vec<gfx_atlas_node>
  void* face;                        // FT_Face internally
  bool added;
  ht(unsigned long, gfx_char) chars;
} gfx_typeface;


struct gfx_ctx {
  GLFWwindow* window;
  vec4 curcol;            // Current color
  uint32_t slots;         // Array of length 32 of booleans for slots(OpenGL only supports 32).
  float z;                // Current "z-index"
  gfx_texture* textures;  // Vec<gfx_texture>
  
  // OpenGL related variables.
  struct {
    GLuint progid, varrid, vbufid, idxbufid;
    GLuint curtex;
    ht(char*, GLint) uniforms;
  } gl;

  struct {
    gfx_typeface* store;
    uint32_t cur;
    uint32_t size;
    uint32_t lh;
  } fonts;

  struct gfx_drawbuf {
    struct gfx_vertexbuf {
      vec2 p;
      float z;
      vec2 tp;
      vec4 col;
    }* shp;
    uint32_t* idx;
    gfx_vector lastsize;
    bool textdrawn;
  } drawbuf;

  struct {
    double x, y;
    struct {
      double x, y;
    } press;
    bool pressed;
  } mouse;

  struct {
		double lastfpscalc;
		double lastfpsnum;
		double lastfpscount;
    double start;
    double delta;
    double last;
    double frametime;
    uint32_t count;
  } frame;

  struct {
    mat4x4 screen;
    mat4x4 cur;
    bool changed;
    bool used;        // Calls draw if there were shapes drawn between transforms
  } transform;
};

static struct gfx_ctx* ctx = NULL;


// -------------------------------- OpenGL Helper Functions + Data --------------------------------

enum gfx_drawprim {
	TEXT = 0, SHAPE = 0, IMAGE = 1
};

static struct {
	const char* vert;
	const char* frag;
} default_shaders = {
	.vert = "#version 330 core\n" GFX_STR(
precision mediump float;
layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 col;

uniform mat4 u_mvp;

out vec2 v_uv;
out vec4 v_col;

void main() {
	gl_Position = u_mvp * pos;
	v_uv = uv;
	v_col = col;
}
),

	.frag = "#version 330 core\n" GFX_STR(
precision mediump float;
layout (location = 0) out vec4 color;

in vec2 v_uv;
in vec4 v_col;

uniform sampler2D u_tex;
uniform int u_shape;

vec4 text;

void main() {
	
	if(u_shape == 0) {
		if(v_uv.x < 0) color = v_col; // Shape rendering
		else color = vec4(texture(u_tex, v_uv).r * v_col.rgba); // Text rendering
	}
	else {
		text = texture(u_tex, v_uv);
		color = vec4(text.rgb, text.a * v_col.a); // Image rendering
	}
	// color = u_shape == 1 ? vec4(text.r * v_col.rgba) : u_shape == 0 ? v_col : vec4(text.rgb, text.a * v_col.a);
	// color = u_shape ? v_col : text;
	// color = vec4(1.0, 1.0, 1.0, 1.0);
}
)
};

struct gfx_layoutelement {
	GLenum type;
	u32 count;
	bool normalized;
};

static inline u32 gfx_glsizeof(GLenum type) {
	switch(type) {
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
			return 1;
		default: return 4;
	}
}

static inline void gfx_applylayout(u32 count, struct gfx_layoutelement* elems) {
	size_t offset = 0, stride = 0;
	for (u32 i = 0; i < count; i ++) stride += elems[i].count * gfx_glsizeof(elems[i].type);
	for (u32 i = 0; i < count; i ++) {
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, elems[i].count, elems[i].type, elems[i].normalized ? GL_TRUE : GL_FALSE, stride, (const void*) offset);
		offset += elems[i].count * gfx_glsizeof(elems[i].type);
	}
}

static GLuint gfx_compshader(GLenum type, const char* src) {

	// Creates a shader in OpenGL, inserts the source and compiles it.
	GLuint id = glCreateShader(type);
	glShaderSource(id, 1, &src, NULL);
	glCompileShader(id);
	
	// Gets the compile status, checks if it failed and then displays a message.
	#ifdef GFX_DEBUG
		int res;
		glGetShaderiv(id, GL_COMPILE_STATUS, &res);
		if(res == GL_FALSE) {
			int length;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
			char* message = "";
			if (length) {
				message = malloc(length * sizeof(char));
				glGetShaderInfoLog(id, length, &length, message);
			}
			error("Failed to compile %s shader!\n%s\n", (type == GL_VERTEX_SHADER ? "vertex" : "fragment"), message);
			glDeleteShader(id); return 0;
		}
	#endif
	
	return id;
}


static GLuint gfx_shaderprog(const char* vert, const char* frag) {
	GLuint program = glCreateProgram(),
		vs = gfx_compshader(GL_VERTEX_SHADER, vert),
		fs = gfx_compshader(GL_FRAGMENT_SHADER, frag);
	
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glValidateProgram(program);

	glDetachShader(program, vs);
	glDetachShader(program, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	return program;
}

// ----------------------------------- Small Font Atlas Library -----------------------------------

// Algorithm's concept picked up from: https://blackpawn.com/texts/lightmaps/default.html
// Resizing texture picked up from here: https://straypixels.net/texture-packing-for-fonts/
// Could use this algorithm with "unused spaces" from here but would require a reimpl: https://github.com/TeamHypersomnia/rectpack2D/
// gfx_atlas_insert(vnew(), &(struct gfx_atlas_node) { {x, y}, {w, h}, {NULL, NULL}, false }, &(gfx_vector) {w, h})
static struct gfx_atlas_node* gfx_atlas_insert(struct gfx_typeface* face, u32 parent, gfx_vector* size) {
	struct gfx_atlas_node* prnt = face->atlas_tree + parent;

	// Index 0 is taken up by the root node.
	if(prnt->child[0] == 0 || prnt->child[1] == 0) {
		
		// If the node is already filled
		if(prnt->filled) return NULL;
		
		// If the image fits perfectly in the node, fill it as this ends the recursion.
		if(prnt->s.w == size->w && prnt->s.h == size->h) {
			prnt->filled = true;
			return prnt;
		}

		// The real size(against the edges of the texture) of the node
		gfx_vector realsize = prnt->s;
		if (prnt->p.x + prnt->s.w == INT_MAX) realsize.w = ctx->textures[face->tex].size.w - prnt->p.x;
		if (prnt->p.y + prnt->s.h == INT_MAX) realsize.h = ctx->textures[face->tex].size.h - prnt->p.y;
		
		// If the image doesn't fit in the node
		if(realsize.x < size->x || realsize.y < size->y) return NULL;
		
		// Defines children
		struct gfx_atlas_node c1 = { .p = prnt->p };
		struct gfx_atlas_node c2 = {};
		
		int dw = realsize.x - size->x;
		int dh = realsize.y - size->y;
		
		// To give the rest of the nodes more space, we need to split where there's more room
		bool vertsplit = dw > dh;
		
		// If the last texture is perfectly going to fit into the last available spot in the texture, split to maximize space (one of them is INT_MAXes)
		if(dw == 0 && dh == 0) vertsplit = prnt->s.w > prnt->s.h;
		
		// Splits the node into two rectangles and assigns them as children
		if (vertsplit) {
			c1.s.x = size->x;
			c1.s.y = prnt->s.y;
			
			c2.p.x = prnt->p.x + size->x;
			c2.p.y = prnt->p.y;
			c2.s.x = prnt->s.x - size->x;
			c2.s.y = prnt->s.y;
		} else {
			c1.s.x = prnt->s.x;
			c1.s.y = size->y;
			
			c2.p.x = prnt->p.x;
			c2.p.y = prnt->p.y + size->y;
			c2.s.x = prnt->s.x;
			c2.s.y = prnt->s.y - size->y;
		}

		// All heap allocs are confined to this, makes it much easier to free.
		vpusharr(face->atlas_tree, { c1, c2 }); // THIS MODIFIES THE POINTER SO YOU CANNOT USE PRNT BECAUSE IT USES THE OLD PTR
		face->atlas_tree[parent].child[0] = vlen(face->atlas_tree) - 2;
		face->atlas_tree[parent].child[1] = vlen(face->atlas_tree) - 1;
		
		return gfx_atlas_insert(face, face->atlas_tree[parent].child[0], size);
	}

	struct gfx_atlas_node* firstchildins = gfx_atlas_insert(face, prnt->child[0], size);
	return firstchildins ? firstchildins : gfx_atlas_insert(face, prnt->child[1], size);
}

// ------------------------------------------ Util Funcs ------------------------------------------

size_t gfx_read(char* file, char** (*buf)) {
	FILE *fp = fopen(file, "rb");
	#ifdef GFX_DEBUG
		if (!fp) {
			error("Couldn't open file %s", file);
			return 0;
		}
	#endif

	// Finds length of file
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	rewind(fp);
	
	*(*buf) = malloc(len * sizeof(char) + 1);
	*(*buf)[len] = '\0';
	fread((*buf), 1, len, fp);
	fclose(fp);

	// returns both
	return len;
}

u8* gfx_readtoimg(char* file, int* width, int* height) {
	int channels;
	// stbi_set_flip_vertically_on_load(true);
	return stbi_load(file, width, height, &channels, STBI_rgb_alpha);
}

// ------------------------- OpenGL Helper Functions that Rely on Context -------------------------

// Gets the location from cache or sets it in cache
GLint glULoc(char* name) {
	GLint* v = hgets(ctx->gl.uniforms, name);
	if(v == NULL) return hsets(ctx->gl.uniforms, name) = glGetUniformLocation(ctx->gl.progid, name);
	return *v;
}

void glUSetm4(char* name, mat4x4 p) { glUniformMatrix4fv(glULoc(name), 1, GL_FALSE, (const GLfloat*) p); }
void glUSeti(char* name, unsigned int p) { glUniform1i(glULoc(name), p); }
void glUSet3f(char* name, vec3 p) { glUniform3f(glULoc(name), p[0], p[1], p[2]); }
void glUSet2f(char* name, vec2 p) { glUniform2f(glULoc(name), p[0], p[1]); }

// --------------------------------------- Input Functions ---------------------------------------

gfx_vector gfx_mouse() {
	double xpos, ypos;
	glfwGetCursorPos(ctx->window, &xpos, &ypos);
	return (gfx_vector) { xpos, ypos };
}

// Creates a projection in proportion to the screen coordinates
void gfx_updatescreencoords(int w, int h) {	
	mat4x4_identity(ctx->transform.screen);
	mat4x4_ortho(ctx->transform.screen, .0f, (f32) w, (f32) h, .0f, 1.0f, -1.f);
	glUSetm4("u_mvp", ctx->transform.screen);
}

void gfx_mousebuttoncallback(GLFWwindow* window, int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		ctx->mouse.pressed = true;
		glfwGetCursorPos(window, &ctx->mouse.press.x, &ctx->mouse.press.y);
	} else ctx->mouse.pressed = false;
}

void gfx_cursorposcallback(GLFWwindow* window, double xpos, double ypos) {
	ctx->mouse.x = xpos;
	ctx->mouse.y = ypos;
	if(ctx->mouse.pressed) {
		int wxpos, wypos;
		glfwGetWindowPos(window, &wxpos, &wypos);
		glfwSetWindowPos(window, (int) (xpos - ctx->mouse.press.x) + wxpos, (int) (ypos - ctx->mouse.press.y) + wypos);
	}
}

void gfx_framebuffersizecallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	gfx_updatescreencoords(width, height);
}




// --------------------------------------- Setup Functions ---------------------------------------

FT_Library ft = NULL;


void gfx_setctx(struct gfx_ctx* c) {
	ctx = c;
	glBindVertexArray(c->gl.varrid);
	glUseProgram(c->gl.progid);
}

struct gfx_ctx* gfx_init(const char* title, u32 w, u32 h) {
	GLFWwindow* window;

	#ifdef GFX_DEBUG
		glfwSetErrorCallback(glfwErrorHandler);
	#endif
	
	// Initializes the library
	CHECK_CALL(!glfwInit(), return NULL, "Couldn't initialize glfw3.");

	// GLFW hints
	#ifdef GFX_DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	#endif
	// glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	// glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

	// Create a windowed mode window and its OpenGL context
	CHECK_CALL(!(window = glfwCreateWindow(w, h, title, NULL, NULL)),
		glfwTerminate(); return NULL, "Couldn't initialize GLFW window.");

	// Make the window's context current
	glfwMakeContextCurrent(window);

	// GLFW input callbacks
	glfwSetMouseButtonCallback(window, gfx_mousebuttoncallback);
	glfwSetCursorPosCallback(window, gfx_cursorposcallback);
	glfwSetFramebufferSizeCallback(window, gfx_framebuffersizecallback);

	// Set the framerate to the framerate of the screen, basically 60fps.
	// glfwSwapInterval(1);

	GLenum err;
	CHECK_CALL((err = glewInit()), glfwTerminate(); return NULL, "GLEW initialization failed: %s", glewGetErrorString(err));

	#ifdef GFX_DEBUG
		glDebugMessageCallback(glDebugMessageHandler, NULL);
	#endif

	// Enables OpenGL Features
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	// glEnable(GL_DEPTH_TEST);
	
	// Fills application context struct
	struct gfx_ctx* ctx = calloc(1, sizeof(struct gfx_ctx));
	ctx->gl.progid = gfx_shaderprog(default_shaders.vert, default_shaders.frag);
	glGenVertexArrays(1, &ctx->gl.varrid);
	ctx->textures = vnew();
	ctx->fonts.store = vnew();
	ctx->fonts.size = 48;
	ctx->z = -1.0f;
	ctx->window = window;

	// Initializes OpenGL Context
	gfx_setctx(ctx);
	gfx_updatescreencoords(w, h);
	fill(255, 255, 255, 255);

	// Initializes draw (*buf)fer vertexes
	ctx->drawbuf.shp = vnew();
	ctx->drawbuf.idx = vnew();
	return ctx;
}

static inline void drawtext();
bool gfx_nextframe() {
	glClear(GL_COLOR_BUFFER_BIT);
	ctx->frame.count ++;
	ctx->frame.start = glfwGetTime();
	ctx->frame.delta = ctx->frame.start - ctx->frame.last;
	ctx->frame.last  = ctx->frame.start;
	
	return !glfwWindowShouldClose(ctx->window);
}
void gfx_frameend() {
	if(vlen(ctx->drawbuf.shp)) drawtext();
	ctx->z = -1.0f;
	glfwSwapBuffers(ctx->window);
	glfwPollEvents();
}

void gfx_close() {
	glfwSetWindowShouldClose(ctx->window, 1);
}
bool gfx_fpschanged() {
	return ctx->frame.start - ctx->frame.lastfpscalc >= .25;
}

double gfx_fps() {
	double rn = ctx->frame.start;
	if (rn - ctx->frame.lastfpscalc < .25 ||
	    ctx->frame.count - ctx->frame.lastfpscount == 0) return ctx->frame.lastfpsnum;
	ctx->frame.lastfpsnum = ((double) ctx->frame.count - ctx->frame.lastfpscount) / (rn - ctx->frame.lastfpscalc);
	ctx->frame.lastfpscount = ctx->frame.count;
	ctx->frame.lastfpscalc = rn;
	return ctx->frame.lastfpsnum;
}


// -------------------------------------- Graphics Functions --------------------------------------

static inline void draw(struct gfx_drawbuf* dbuf, u32 slen, u32 ilen) {
	if(!slen) return;

	if(!ctx->gl.vbufid) {
		glGenBuffers(1, &ctx->gl.vbufid);
		glBindBuffer(GL_ARRAY_BUFFER, ctx->gl.vbufid);
		glGenBuffers(1, &ctx->gl.idxbufid);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->gl.idxbufid);
		
		// Applies layout
		gfx_applylayout(3, (struct gfx_layoutelement[]) {
			{ GL_FLOAT, 3, false }, // X, Y, Z
			{ GL_FLOAT, 2, false }, // Texture X, Texture Y
			{ GL_FLOAT, 4, false }  // Color (RGBA)
		});
	}

	// Vertex buffer upload
	if(dbuf->lastsize.w < slen)
		glBufferData(GL_ARRAY_BUFFER, slen * sizeof(*dbuf->shp), dbuf->shp, GL_DYNAMIC_DRAW), dbuf->lastsize.w = slen;
	else glBufferSubData(GL_ARRAY_BUFFER, 0, slen * sizeof(*dbuf->shp), dbuf->shp);
	
	// Element array buffer upload
	if(dbuf->lastsize.h < ilen)
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ilen * sizeof(*dbuf->idx), dbuf->idx, GL_DYNAMIC_DRAW),
		dbuf->lastsize.h = ilen;
	else glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, ilen * sizeof(*dbuf->idx), dbuf->idx);

	// Draw call
	glDrawElements(GL_TRIANGLES, ilen, GL_UNSIGNED_INT, NULL);

	// Reset draw buffers and Z axis
	if(dbuf == &ctx->drawbuf)
		vempty(dbuf->shp), vempty(dbuf->idx);
}



void fill(u8 r, u8 g, u8 b, u8 a) {
	ctx->curcol[0] = (f32) r / 255.f;
	ctx->curcol[1] = (f32) g / 255.f;
	ctx->curcol[2] = (f32) b / 255.f;
	ctx->curcol[3] = (f32) a / 255.f;
}

void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
	u32 curidx = vlen(ctx->drawbuf.shp);
	vpusharr(ctx->drawbuf.idx, { curidx, curidx + 1, curidx + 2, curidx + 2, curidx, curidx + 3 });
	vpusharr(ctx->drawbuf.shp, {
		{ .p = { (f32) x1, (f32) y1 }, .z = (ctx->z += .001f), .tp = { -1.f, -1.f }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { (f32) x2, (f32) y2 }, .z = (ctx->z         ), .tp = { -1.f, -1.f }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { (f32) x3, (f32) y3 }, .z = (ctx->z         ), .tp = { -1.f, -1.f }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { (f32) x4, (f32) y4 }, .z = (ctx->z         ), .tp = { -1.f, -1.f }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } }
	});
}

void rect(int x, int y, int w, int h) {
	quad(x, y, x + w, y, x + w, y + h, x, y + h);
}


// Sets texture lookup coordinates
static inline void gfx_tp(f32* texcoords) {
	struct gfx_vertexbuf* last4 = vlast(ctx->drawbuf.shp) - 3;
	for(int i = 0; i < 4; i ++)
		last4[i].tp[0] = texcoords[i * 2],
		last4[i].tp[1] = texcoords[i * 2 + 1];
}


static inline u32 gfx_inittex(u8* tex, u32 width, u32 height, gfx_texture_type type) {
	vpush(ctx->textures, { tex, { width, height }, 0, -1, 0, type });
	return vlen(ctx->textures) - 1;
}

// https://stackoverflow.com/a/21624571/10013227
#if defined(_MSC_VER) && !defined(__clang__)
	static inline u32 firstbit(x) {
		unsigned long index;
		_BitScanForward(&index, x);
		return index;
	}
#else
	#if __has_builtin(__builtin_ffs)
		#define firstbit(x) __builtin_ffs(x)
	#else
		static const int MultiplyDeBruijnBitPosition[32] = 
		{
			0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
			31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
		};
		#define firstbit(c) MultiplyDeBruijnBitPosition[((uint32_t)((c & -c) * 0x077CB531U)) >> 27];
	#endif
#endif

static inline u32 findslot() {
	u32 slot = firstbit(~ctx->slots) - 1;
	if(slot < 0) {
		for(int i = 0; i < vlen(ctx->textures); i ++)
			if(ctx->textures[i].slot == 31) ctx->textures[i].slot = -1;
		return 31;
	}
	ctx->slots |= 1 << slot;
	return slot;
}


static inline void gfx_setcurtex(img tex) {
	ctx->textures[tex].uses++;
	if(ctx->gl.curtex == ctx->textures[tex].slot) return;
	glUSeti("u_tex", ctx->textures[tex].slot);
	ctx->gl.curtex = ctx->textures[tex].slot;
}

static inline void gfx_activetex(img tex) {
	ctx->textures[tex].slot = findslot();
	glActiveTexture(GL_TEXTURE0 + ctx->textures[tex].slot);
}

static inline void gfx_texupload(img tex) {
	gfx_activetex(tex);
	
	gfx_texture* t = ctx->textures + tex;
	gfx_texture_type type = t->type;
	GLenum format;

	if(type == GFX_TEX_TYPEFACE)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1), format = GL_RED;
	else glPixelStorei(GL_UNPACK_ALIGNMENT, 4), format = GL_RGBA;
	
	// Generates and binds the texture
	glGenTextures(1, &t->id);
	glBindTexture(GL_TEXTURE_2D, t->id);
	
	// Sets default params
	GLenum minmagfilter = type == GFX_TEX_PIXELART ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minmagfilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, minmagfilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Sends texture data to the gpu
	glTexImage2D(GL_TEXTURE_2D, 0, format, t->size.w, t->size.h, 0, format, GL_UNSIGNED_BYTE, t->tex);
	
	// We just don't need mipmaps for fonts so we do this for normal images
	if(type == GL_TEX_IMAGE) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glGenerateMipmap(GL_TEXTURE_2D); // CALL AFTER UPLOAD
	}
}

static inline void gfx_texupdate(img tex) {
	glActiveTexture(GL_TEXTURE0 + ctx->textures[tex].slot);
	gfx_texture* t = ctx->textures + tex;
	GLenum format = t->type == GFX_TEX_TYPEFACE ? GL_RED : GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0, format, t->size.w, t->size.h, 0, format, GL_UNSIGNED_BYTE, t->tex);
}

img gfx_loadimg(char* file) {
	int width, height;
	u8* t; CHECK_CALL(!(t = gfx_readtoimg(file, &width, &height)), return -1, "Couldn't load image '%s'", file);
	info("Loaded image '%s' (%dx%d)", file, width, height);
	return gfx_inittex(t, width, height, GL_TEX_IMAGE);
}


void image(img img, int x, int y, int w, int h) {
	if(img < 0) return;
	
	// Sets up the environment
	// if(vlen(ctx->drawbuf.shp)) glUSeti("u_shape", SHAPE), draw(&ctx->drawbuf, vlen(ctx->drawbuf.shp), vlen(ctx->drawbuf.idx));
	if(!ctx->textures[img].id) gfx_texupload(img);
	else if(ctx->textures[img].slot < 0) gfx_activetex(img);
	gfx_setcurtex(img);

	// Creates a rectangle the image will be held on, then uploads the texture coordinates to map to the image
	struct gfx_drawbuf d = {
		.shp = (struct gfx_vertexbuf[]) {
			{{ x    , y     }, (ctx->z += .001f), { 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }}, // ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ x + w, y     }, (ctx->z         ), { 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }}, // ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ x + w, y + h }, (ctx->z         ), { 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }}, // ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ x    , y + h }, (ctx->z         ), { 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }}  // ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }}
		},
		.idx = (u32[]) { 0, 1, 2, 2, 0, 3 },
		.lastsize = ctx->drawbuf.lastsize
	};
	
	glUSeti("u_shape", 2);
	draw(&d, 4, 6);
}

gfx_vector* isize(img img) { return &ctx->textures[img].size; }

/*
	Text drawing process:
		Load Stage:
			- gfx_loadfont()
				- Creates Freetype initial context and Face
				- Loads a space but doesn't do anything except update the space_width prop
				- Adds the alphabet to the atlas(atlas not rendered yet)
					- Calls gfx_loadfontchar() in a loop
		
		Draw Loop:
			- text()
				- When creating Index + Vertex (*buf)fers, check for unloaded chars and load them
			- gfx_loadfont()
				- Same thing as load
			- gfx_draw() call
				- Loop through every font, determine which need to be updated
				- First draw() call:
					- Call gfx_createfontatlastex()
				- Subsequent draw() calls:
					- Check if there were any new characters loaded, and call gfx_updatefontatlastex()
*/

static inline FT_ULong gfx_readutf8(u8** str) {
	if(**str == 0) return 0;
	FT_ULong code_point = 0;

	int len =
		**str > 0x80 ?
			(**str & 0xE0) == 0xE0 ? 2 :
				(**str & 0xC0) == 0xC0 ? 1 : 3
		: 0;

	if(len > 0) **str <<= len + 2, **str >>= len + 2;
	code_point += *(*str)++;
	while (len--) code_point = code_point << 6 | (*(*str)++ & 0x3F);
	
	return code_point;
}

static gfx_char* gfx_loadfontchar(typeface tf, FT_ULong c) {
	gfx_typeface* face = ctx->fonts.store + tf;
	gfx_texture* tex = ctx->textures + face->tex;
	CHECK_CALL(FT_Load_Char(face->face, c, FT_LOAD_RENDER), return NULL, "Couldn't load char '%c' (%X)", c, c);
	
	int width = ((FT_Face) face->face)->glyph->bitmap.width;
	int height = ((FT_Face) face->face)->glyph->bitmap.rows;
	gfx_vector size = { width, height };

	// Tries to add the character, resizing the whole texture until it's done
	struct gfx_atlas_node* maybe;
	int growth = 1;
	while(!(maybe = gfx_atlas_insert(face, 0, &size))) {
		info("Atlas size doubled for font '%s'", face->name);
		growth *= 2, tex->size.w *= 2, tex->size.h *= 2;
	}

	if(!tex->tex)
		tex->tex = malloc(tex->size.w * tex->size.h * sizeof(u8));
	else if(growth > 1) {
		int ow = tex->size.w / growth, oh = tex->size.h / growth;
		u8* ntex = malloc(tex->size.w * tex->size.h * sizeof(u8));
		u8* otex = tex->tex;
		
		for(int i = 0; i < oh; i ++)
			memcpy(ntex + i * tex->size.w, otex + i * ow, ow);

		tex->tex = ntex;
		free(otex);
	}

	// Writes the character's pixels to the atlas buffer.
	int x = maybe->p.x, y = maybe->p.y;
	int w = size.w,     h = size.h;
	u8* t = tex->tex + x + y * tex->size.w;
	for(int i = 0; i < h; i ++)
		memcpy(t + i * tex->size.w, ((FT_Face) face->face)->glyph->bitmap.buffer + i * w, w);

	face->added = true;
	sizeof(**(face->chars).keys);
	hset(face->chars, c) = (gfx_char) {
		.c = c,
		.size = size,
		.bearing = (gfx_vector) {
			((FT_Face) face->face)->glyph->bitmap_left,
			((FT_Face) face->face)->glyph->bitmap_top
		},
		.advance = ((FT_Face) face->face)->glyph->advance.x >> 6,
		.place = maybe->p
	};
	return hlastv(face->chars);
}

typeface gfx_loadfont(const char* file) {

	gfx_typeface new = {
		.name = malloc(strlen(file) + 1),
		.atlas_tree = vnew(),
		.tex = gfx_inittex(NULL,
			GFX_FONT_ATLAS_START_SIZE,
			GFX_FONT_ATLAS_START_SIZE,
			GFX_TEX_TYPEFACE),
		.chars = {0},
		.face = NULL
	};
	memcpy(new.name, file, strlen(file) + 1);
	vpush(new.atlas_tree, { .s = { INT_MAX, INT_MAX } });
	
	// Loads the freetype library.
	if(!ft)
		CHECK_CALL(FT_Init_FreeType(&ft), return -1, "Couldn't initialize freetype");

	// Loads the new face in using the library
	CHECK_CALL(FT_New_Face(ft, file, 0, (FT_Face*) &new.face), return -1, "Couldn't load font '%s'", file);
	FT_Set_Pixel_Sizes(new.face, 0, 16);

	// Adds space_width
	CHECK_CALL(FT_Load_Char(new.face, ' ', FT_LOAD_RENDER), return -1, "Couldn't load the Space Character ( ) ");
	new.space_width = ((FT_Face) new.face)->glyph->advance.x >> 6;

	// Stores the font
	vpush(ctx->fonts.store, new);
	info("Loaded font '%s'", new.name);
	return vlen(ctx->fonts.store) - 1;
}

void fontsize(u32 size) { ctx->fonts.size = size; }
void lineheight(f32 h) { ctx->fonts.lh = h; }

// Draws any leftover shapes and text
static inline void drawtext() {
	if(!vlen(ctx->fonts.store)) goto draw;
	gfx_typeface* face = ctx->fonts.store + ctx->fonts.cur;
	gfx_texture* tex = ctx->textures + face->tex;
	
	if(!tex->id) {
		stbi_write_png("bitmap.png", tex->size.w, tex->size.h, 1, tex->tex, tex->size.w);
		gfx_texupload(face->tex);
	} else if(face->added) {
		stbi_write_png("bitmap.png", tex->size.w, tex->size.h, 1, tex->tex, tex->size.w);
		gfx_texupdate(face->tex);
		face->added = false;
	}
	else if(tex->slot < 0) gfx_activetex(face->tex);
	gfx_setcurtex(face->tex);

draw:
	glUSeti("u_shape", TEXT);
	draw(&ctx->drawbuf, vlen(ctx->drawbuf.shp), vlen(ctx->drawbuf.idx));
}
void font(typeface face, u32 size) {
	
	// If you're setting to the same font, just return preemptively
	if(face == ctx->fonts.cur) goto end;
	
	// Draws anything left in the shape buffer from previous fonts
	if(vlen(ctx->drawbuf.shp) && ctx->drawbuf.textdrawn) drawtext();
	
	ctx->fonts.cur = face;
	ctx->drawbuf.textdrawn = false;
end:
	if(size > 0) ctx->fonts.size = size;
}
void text(const char* str, int x, int y) {
	if(!vlen(ctx->fonts.store)) return;

	gfx_typeface* face = ctx->fonts.store + ctx->fonts.cur;
	gfx_texture* tex = ctx->textures + face->tex;
	f32 scale = (f32) ctx->fonts.size * 4.0f / 3.0f /*px -> pts*/ / 16.0f;
	ctx->z += .001f;

	FT_ULong point;
	int curx = x, cury = y;
	f32 tx, ty, tw, th, realx, realy, w, h;
	while ((point = gfx_readutf8((u8**) &str))) {

		// Newlines in five lines :D
		if (point == '\n') {
			y += (int) (ctx->fonts.lh * ctx->fonts.size * 4.0f / 3.0f);
			curx = x;
			continue;
		}

		else if(point == ' ') {
			curx += face->space_width * scale;
			continue;
		}
		
		gfx_char* ch = hget(face->chars, point);
		if(!ch) ch = gfx_loadfontchar(ctx->fonts.cur, point);

		realx = (f32) curx + ch->bearing.x * scale;
		realy = (f32) cury - ch->bearing.y * scale;
		w     = (f32) ch->size.w * scale;
		h     = (f32) ch->size.h * scale;

		tx = ch->place.x / (f32) tex->size.w;
		ty = ch->place.y / (f32) tex->size.h;
		tw = ch->size.w  / (f32) tex->size.w;
		th = ch->size.h  / (f32) tex->size.h;
		
		// Kind of copied from quad() so it can be more efficient with less int -> float -> int conversions.
		u32 curidx = vlen(ctx->drawbuf.shp);
		vpusharr(ctx->drawbuf.idx, { curidx, curidx + 1, curidx + 2, curidx + 2, curidx, curidx + 3 });
		vpusharr(ctx->drawbuf.shp, {
			{{ realx    , realy     }, ctx->z, { tx     , ty      }, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ realx + w, realy     }, ctx->z, { tx + tw, ty      }, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ realx + w, realy + h }, ctx->z, { tx + tw, ty + th }, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ realx    , realy + h }, ctx->z, { tx     , ty + th }, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }}
		});

		// Advance cursors for next glyph
		curx += (float) ch->advance * scale;
	}
	
	ctx->drawbuf.textdrawn = true;
}


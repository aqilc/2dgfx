/**
 * TODO:
 * 	- Text Rendering
 * 		- tsiz()
 * 		- setfont()
 * 		- text()
 * 	- Masking
 * 		- Determine Structure for drawing.
 * 		- https://community.khronos.org/t/texture-environment/39767/4
 */

#include "2dgfx.h"

#include <stdint.h>

#include <vec.h>
#include <hash.h>
#include <linmath.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define GFX_STR(...) #__VA_ARGS__
#define GFX_STR_MACRO(...) GFX_STR(__VA_ARGS__)

#ifndef GFX_FONT_ATLAS_START_SIZE
	#define GFX_FONT_ATLAS_START_SIZE 512
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
	#include <stdio.h>
	#include <stdarg.h>
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include <stb/stb_image_write.h>
	
	static inline void error_(const char* line, const char* file, const char* err, ...) {
		va_list args;
		va_start(args, err);
		printf("2DGFX Internal Error: ");
		vprintf(err, args);
		printf(" (Line: %s | File: %s)\n", line, file);
		va_end(args);
	}

	#define error(...) error_(GFX_STR_MACRO(__LINE__), __FILE__, __VA_ARGS__)

	#define CHECK_CALL(call, ...) if(call) error(__VA_ARGS__)
	
	void GLAPIENTRY glDebugMessageHandler(GLenum source,
			GLenum type, GLuint id, GLenum severity, GLsizei length,
			const GLchar* message, const void* userParam) {
		printf("        OpenGL Error: %s (Code: %d)\n", message, type);
	}
#else
	#define error(...) 
	#define CHECK_CALL(call, ...) call
#endif


// -------------------------------- OpenGL Helper Functions + Data --------------------------------

static struct {
	const char* vert;
	const char* frag;
} default_shaders = {
	.vert = "#version 330 core\n" GFX_STR(
precision mediump float;
layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 txt;
layout (location = 2) in vec4 col;

uniform mat4 u_mvp;

out vec2 v_text;
out vec4 v_col;

void main() {
	gl_Position = u_mvp * pos;
	v_text = txt;
	v_col = col;
}),

	.frag = "#version 330 core\n" GFX_STR(
precision mediump float;
layout (location = 0) out vec4 color;

in vec2 v_text;
in vec4 v_col;

uniform sampler2D u_tex;
uniform bool u_shape;

vec4 text;

void main() {
	text = texture(u_tex, v_text);
	// color = u_shape ? vec4(text.r * v_col.rgba) : vec4(mix(text.rgb, v_col.rgb, v_col.a), text.a);
	color = u_shape ? v_col : text;
	// color = vec4(1.0, 1.0, 1.0, 1.0);
})
};

struct glLayoutElement {
	GLenum type;
	u32 count;
	bool normalized;
};

static inline u32 glTypeSize(GLenum type) {
	switch(type) {
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
			return 1;
		default: return 4;
	}
}

static inline void glLayoutApply(u32 count, struct glLayoutElement* elems) {
	size_t offset = 0, stride = 0;
	for (u32 i = 0; i < count; i ++) stride += elems[i].count * glTypeSize(elems[i].type);
	for (u32 i = 0; i < count; i ++) {
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, elems[i].count, elems[i].type, elems[i].normalized ? GL_TRUE : GL_FALSE, stride, (const void*) offset);
		offset += elems[i].count * glTypeSize(elems[i].type);
	}
}

static GLuint glCompileSrc(GLenum type, const char* src) {

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

static GLuint glProgram(const char* vert, const char* frag) {
  GLuint program = glCreateProgram(),
    vs = glCompileSrc(GL_VERTEX_SHADER, vert),
    fs = glCompileSrc(GL_FRAGMENT_SHADER, frag);
  
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

static GLuint glUploadTexture(u8* tex, u32 width, u32 height, gfx_texture_type type) {
	GLuint t;
  
  // Generates and binds the texture
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  
  // Sets default params
  GLenum minmagfilter = type == GFX_TEX_PIXELART ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minmagfilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, minmagfilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Sends texture data to the gpu
  GLenum format = type == GFX_TEX_TYPEFACE ? GL_RED : GL_RGBA;
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, tex);

  // We just don't need mipmaps for fonts so we do this for normal images
  if(type == GL_TEX_IMAGE) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  
  return t;
}

// ----------------------------------- Small Font Atlas Library -----------------------------------

// gfx_atlas_insert(vnew(), &(struct gfx_atlas_node) { {x, y}, {w, h}, {NULL, NULL}, false }, &(gfx_vector) {w, h})
static struct gfx_atlas_node* gfx_atlas_insert(struct gfx_atlas_node* buf, struct gfx_atlas_node* parent, gfx_vector* size) {
  if(parent->child[0] == NULL || parent->child[1] == NULL) {
    
    // If the node is already filled
    if(parent->filled) return NULL;
    
    // If the image doesn't fit in the node
    if(parent->s.x < size->x || parent->s.y < size->y) return NULL;
    
    // If the image fits perfectly in the node, return the node since we can't really split anymore anyways
    if(parent->s.x == size->x && parent->s.y == size->y) { parent->filled = true; return parent; }
    
		// Splits node
		struct gfx_atlas_node c1 = {
			.p = { parent->p.x, parent->p.y }
		};
		struct gfx_atlas_node c2 = {0};
		
		int dw = parent->s.x - size->x;
		int dh = parent->s.y - size->y;
		
		// Sets everything that would be the same for both type of splits
		c1.p.x = parent->p.x;
		c1.p.y = parent->p.y;

		// If the margin between the right of the rect and the edge of the parent rectangle is larger than the margin of the bottom of the rect and the bottom of the parent node
		if(dw > dh) {
      // Vertical Split
      c1.s.x = size->x; // - 1
      c1.s.y = parent->s.y;
      
      c2.p.x = parent->p.x + size->x;
      c2.p.y = parent->p.y;
      c2.s.x = parent->s.x - size->x;
      c2.s.y = parent->s.y;
      
		} else {
      // Horizontal Split
      c1.s.x = parent->s.x;
      c1.s.y = size->y; // - 1
      
      c2.p.x = parent->p.x;
      c2.p.y = parent->p.y + size->y;
      c2.s.x = parent->s.x;
      c2.s.y = parent->s.y - size->y;
    }

		// All heap allocs are confined to this, makes it much easier to free.
    vpush(buf, c1);
		struct gfx_atlas_node* c1p = parent->child[0] = vlast(buf);
    vpush(buf, c2);
		parent->child[1] = vlast(buf);
    
    return gfx_atlas_insert(buf, c1p, size);
  }
  
  struct gfx_atlas_node* firstchildres = gfx_atlas_insert(buf, parent->child[0], size);
  if(firstchildres == NULL)
    return gfx_atlas_insert(buf, parent->child[1], size);
  return firstchildres;
}

// ------------------------------------------ Util Funcs ------------------------------------------

size_t gfx_read(char* file, char** buf) {
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
  
  *buf = malloc(len * sizeof(char) + 1);
  *buf[len] = '\0';
  fread(buf, 1, len, fp);
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

static struct gfx_ctx* ctx = NULL;


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

// -------------------------------------- Main GFX Functions --------------------------------------

FT_Library ft = NULL;


void gfx_setctx(struct gfx_ctx* c) {
	ctx = c;
	glBindVertexArray(c->gl.varrid);
	glUseProgram(c->gl.progid);
}

struct gfx_ctx* gfx_init(u32 w, u32 h) {
	#ifdef GFX_DEBUG
		glDebugMessageCallback(glDebugMessageHandler, NULL);
	#endif

	// Enables blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	
	// Fills application context struct
	struct gfx_ctx* ctx = calloc(1, sizeof(struct gfx_ctx));
	ctx->gl.progid = glProgram(default_shaders.vert, default_shaders.frag);
	glGenVertexArrays(1, &ctx->gl.varrid);
	ctx->textures = vnew();

	// Initializes OpenGL Context
	gfx_setctx(ctx);

	// Initializes draw buffer vertexes
	ctx->drawbuf.svec = vnew();
	ctx->drawbuf.idvec = vnew();

  // Creates a projection in proportion to the screen coordinates
  mat4x4 p;
  mat4x4_identity(p);
  mat4x4_ortho(p, .0f, (f32) w, (f32) h, .0f, 1.0f, -1.f);
  glUSetm4("u_mvp", p);
  
	return ctx;
}

void draw() {
	if(!vlen(ctx->drawbuf.svec)) return;

	if(!ctx->gl.vbufid) {
		glGenBuffers(1, &ctx->gl.vbufid);
		glBindBuffer(GL_ARRAY_BUFFER, ctx->gl.vbufid);
		glGenBuffers(1, &ctx->gl.idxbufid);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->gl.idxbufid);
		
		// Applies layout
		glLayoutApply(3, (struct glLayoutElement[]) {
			{ GL_FLOAT, 2, false }, // X, Y
			{ GL_FLOAT, 2, false }, // Texture X, Texture Y
			{ GL_FLOAT, 4, false }  // Color (RGBA)
		});
	}
	
	glBufferData(GL_ARRAY_BUFFER, vlen(ctx->drawbuf.svec) * sizeof(*ctx->drawbuf.svec), ctx->drawbuf.svec, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, vlen(ctx->drawbuf.idvec) * sizeof(*ctx->drawbuf.idvec), ctx->drawbuf.idvec, GL_DYNAMIC_DRAW);
	glDrawElements(GL_TRIANGLES, vlen(ctx->drawbuf.idvec), GL_UNSIGNED_INT, NULL);

	vempty(ctx->drawbuf.svec);
	vempty(ctx->drawbuf.idvec);
}



static gfx_texture* gfx_inittex(u8* tex, u32 width, u32 height, gfx_texture_type type) {
	gfx_texture t = { tex, { width, height }, 0, -1, 0, type };
	vpush(ctx->textures, t);
	return vlast(ctx->textures);
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

// TODO: Return Least used image when there are no free slots.
static inline u32 findslot() {
	u32 slot = firstbit(~ctx->slots) - 1;
	if(slot < 0) {
		for(int i = 0; i < vlen(ctx->textures); i ++)
			if(ctx->textures[i].slot == 31) ctx->textures[i].slot = -1;
		return 31;
	}
	return slot;
}

static inline void gfx_texbind(gfx_texture* tex) {
	glActiveTexture(GL_TEXTURE0 + tex->slot);
	glBindTexture(GL_TEXTURE_2D, tex->id);
}

static inline void gfx_texupload(gfx_texture* tex) {
	tex->id = glUploadTexture(tex->tex, tex->size.w, tex->size.h, tex->type);
	tex->slot = findslot();
	gfx_texbind(tex);
}

img gfx_loadimg(char* file) {
	int width, height;
	u8* t = gfx_readtoimg(file, &width, &height);
	return gfx_inittex(t, width, height, GL_TEX_IMAGE);
}


// Sets texture lookup coordinates
static inline void gfx_tp(f32* texcoords) {
	struct gfx_drawbuf* last4 = vlast(ctx->drawbuf.svec) - 3;
	for(int i = 0; i < 4; i ++)
		last4[i].tp = (gfx_vector) { .fx = texcoords[i * 2], .fy = texcoords[i * 2 + 1] };
}


void image(img img, int x, int y, int w, int h) {
	if(vlen(ctx->drawbuf.svec)) glUSeti("u_shape", true), draw();
	if(!img->id) gfx_texupload(img);
	else if(img->slot < 0) gfx_texbind(img);

	img->uses ++;

	// Creates a rectangle the image will be held on, then uploads the texture coordinates to map to the image
	fill(255, 255, 255, 255);
	rect(x, y, w, h);
	gfx_tp((f32[]) { 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f });
	
	glUSeti("u_shape", false);
	draw();
}

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
				- When creating Index + Vertex buffers, check for unloaded chars and load them
			- gfx_loadfont()
				- 
			- gfx_draw() call
				- Loop through every font, determine which need to be updated
				- First draw() call:
					- Call gfx_createfontatlastex()
				- Subsequent draw() calls:
					- Check if there were any new characters loaded, and call gfx_updatefontatlastex()
*/

static FT_ULong gfx_readutf8(u8** str) {
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

static void gfx_loadfontchar(typeface* tf, FT_ULong c) {
	CHECK_CALL(FT_Load_Char(tf->face, c, FT_LOAD_RENDER), "Couldn't load char '%c' (%X)", c, c);

	int width = ((FT_Face) tf->face)->glyph->bitmap.width;
	int height = ((FT_Face) tf->face)->glyph->bitmap.rows;
	gfx_vector size = { width, height };

	// Tries to add the character, resizing the whole texture until it's done
	struct gfx_atlas_node* maybe;
	while(!(maybe = gfx_atlas_insert(tf->atlas_tree,
				&(struct gfx_atlas_node) { .s = tf->tex->size },
				&size)))
		tf->tex->size.w *= 2,
		tf->tex->size.h *= 2;

	hset(tf->chars, c) = (gfx_char) {
		.c = c,
		.size = size,
		.bearing = (gfx_vector) {
			((FT_Face) tf->face)->glyph->bitmap_left,
			((FT_Face) tf->face)->glyph->bitmap_top
		},
		.advance = ((FT_Face) tf->face)->glyph->advance.x >> 6,
		.place = maybe->p
	};
}

static void gfx_createfontatlastex() {
	
}



const char* alphabet = u8"abcdefghijklmnopqrstuvwxyz";
typeface* gfx_loadfont(char* file) {

	typeface new = {
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
	
  // Loads the freetype library.
  if(!ft && FT_Init_FreeType(&ft)) {
    error("Couldn't init freetype"); return NULL; }

  // Loads the new face in using the library
  if(FT_New_Face(ft, file, 0, (FT_Face*) &new.face)) {
    printf("Couldn't load font '%s'", file); return NULL; }
  FT_Set_Pixel_Sizes(new.face, 0, 48);

	// Adds space_width
	CHECK_CALL(FT_Load_Char(new.face, ' ', FT_LOAD_RENDER), "Couldn't load char the Space Character ( ) ");
	new.space_width = ((FT_Face) new.face)->glyph->advance.x >> 6;

	// Loads the alphabet
	for(const char* i = alphabet; *i; i++)
		gfx_loadfontchar(&new, *i);

	hsets(ctx->fonts.store, ((FT_Face) new.face)->family_name) = new;
	return hlastv(ctx->fonts.store);
}

void fill(u8 r, u8 g, u8 b, u8 a) {
	ctx->curcol[0] = (f32) r / 255.f;
	ctx->curcol[1] = (f32) g / 255.f;
	ctx->curcol[2] = (f32) b / 255.f;
	ctx->curcol[3] = (f32) a / 255.f;
}

void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
  vpusharr(ctx->drawbuf.svec, {
		{ .p = { .fx = (f32) x1, .fy = (f32) y1 }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { .fx = (f32) x2, .fy = (f32) y2 }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { .fx = (f32) x3, .fy = (f32) y3 }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { .fx = (f32) x4, .fy = (f32) y4 }, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } }
	});
  vpusharr(ctx->drawbuf.idvec, { 0, 1, 2, 2, 0, 3 });
}

void rect(int x, int y, int w, int h) {
	quad(x, y, x + w, y, x + w, y + h, x, y + h);
}

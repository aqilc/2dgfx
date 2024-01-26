/**
	MODERN OPENGL: https://github.com/fendevel/Guide-to-Modern-OpenGL-Functions/blob/master/README.md#glcreatetextures
		- Simplified, faster, less binding.
		- Cannot support OpenGL ES, requires 4.5-4.6.
		- Used like vulkan: https://developer.nvidia.com/opengl-vulkan
		- https://registry.khronos.org/OpenGL-Refpages/gl4/html/glCopyImageSubData.xhtml

	ECS:
		https://austinmorlan.com/posts/entity_component_system/

	OTHER UI RENDERERS:
		egui: https://github.com/emilk/egui/blob/master/crates/epaint/src/tessellator.rs
		nanogui: https://github.com/mitsuba-renderer/nanogui/blob/master/src/renderpass_gl.cpp
		tgfx: https://github.com/Tencent/tgfx

	CIRCLES:
		Produces vertices: https://github.com/grimfang4/sdl-gpu/blob/master/src/renderer_shapes_GL_common.inl#L360
		Calculates dt, number of points: https://github.com/grimfang4/sdl-gpu/blob/master/src/renderer_shapes_GL_common.inl#L82
		Uses precomputed sin and cos values: https://github.com/emilk/egui/blob/master/crates/epaint/src/tessellator.rs#L337

	DRAWING WITHOUT A WINDOW FOR OFFSCREEN GPU ACCELERATION:
		https://community.khronos.org/t/offscreen-rendering-without-a-window/57842/2
		https://stackoverflow.com/questions/2896879/windowless-opengl
		https://stackoverflow.com/a/48947103/10013227
		
	DEBUGGING RESOURCES:
	- https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/getting-started-with-windbg#summary-of-commands
	- https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/commands
	- https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/application-verifier
	- https://stackoverflow.com/questions/47711390/address-sanitizer-like-functionality-on-msvc
	
	TODO:
		Next:
			- [ ] Front to back drawing
			- [x] Global Text Atlas
		Bugs:
			- [x] While lines when rendering text.
		- Depth Buffer
			- Z COORDINATES FOR EVERYTHING SO IT'S ORDERED
		- Multithreading capabilities -> multiple windows.
			- gfx_window()
		- Shapes
			- [ ] rrect(x, y, w, h, r)
			- [x] circle(x, y, r)
			- [ ] ellipse(x, y, w, h)
			- [x] rect()
			- [x] quad()
		- Clipping
			- [ ] push()
			- [ ] clip(x, y, w, h)
			- https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm
			- Ideas:
				- I might just be checking if points are going out of the current rectangle and change them to be inside.
					push() would have to start its own draw buffer for that, and there would be a need to handle buffer heirarchy and state for them.
				- Stencil buffer usage somehow. Might be much more expensive for overall output, but a large benefit when there are a lot of objects.
				- Integrate clipping regions into every drawing function. This would prevent things outside the screen getting drawn completely.
		- Images
			- [-] findslot()
				- [ ] Return Least used image when there are no free slots.
			- [-] image(img, x, y, w, h)
				- [x] Main functionality
				- [ ] Rendering vector graphics with image by using special IDs for them.
			- [x] gfx_loadimg(char*)
			- Spritesheets
				- [ ] sprites spritesheet(img)
				- [ ] sprite  sprite(sprites, x, y, w, h)
				- [ ] spritegrid spritegrid(sprites, swidth, sheight, columns, )
		- Vector Graphics:
			- [ ] path(&cmds array)
				- [ ] cmds: { MOVE, x, y }, { CUBIC, x, y, x2, y2 }, { LINE, x, y }
				- Internal function: gfx_drawpath(path, width, height)
			- [ ] `image()` integration.
		- Text Rendering
			- [x] fontsize()
			- [x] font()
			- [-] text()
				- [x] Fix bugs
				- [ ] EMOJIS!!!
				- [ ] Subpixel Antialiasing
					- Shader side if possible
				- [x] Dynamic loading of different font sizes and fonts in one combined texture atlas.
			- [ ] Glyph based loading and rendering.
			- [ ] Font Fallbacks
				- gfx_register_fallbacks(int num, { "fallback1", "fallback2" })
			- [ ] Libature Integration (https://github.com/lite-xl/lite-xl/blob/f837c83e552a45773a2371370855c4b1f8395f9b/src/renderer.c#L127)
			- Side Projects:
				- [ ] SDF Based font renderer [IF POSSIBLE, there are way too many drawbacks for it to be an option right NOW].
				- [ ] Not using Freetype, but rendering fonts natively.
			- https://github.com/mightycow/Sluggish/blob/master/code/renderer_gl/main.cpp
			- https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
		- Masking
			- Determine API for drawing.
			- Dynamically turn on stencil testing
			- https://community.khronos.org/t/texture-environment/39767/4
 */

/*
	Lessons Learned:
		- Call glGenerateMipmaps AFTER uploading texture.
		- Empty slot has to be active for texture upload.
*/

#include <stdint.h>
#include <stdio.h>

// GLEW, for opengl functions
#define GLEW_STATIC
#include <GL/glew.h>

// Window management
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define HASH_H_IMPLEMENTATION
#include <hash.h>

#include <math.h>
#include <linmath.h>

#include "2dgfx.h"

#define FLOAT_PRECION_BASED_MAX 16777215.0f

#define GFX_STR(...) #__VA_ARGS__
#define GFX_STR_MACRO(...) GFX_STR(__VA_ARGS__)
#define GLSL(...) "#version 330 core\n" #__VA_ARGS__


// Batch rendering macros
#define GFX_ATLAS_START_SIZE 256
#define GFX_ATLAS_MAX_SIZE 2048
#define RENDERING_FONT_SIZE(...) 48##__VA_ARGS__

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

// #define GFX_DEBUG
#ifdef GFX_DEBUG
	// #define TRACY_ENABLE
	#include <stdarg.h>
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include <stb/stb_image_write.h>
	
	static inline void error_(const char* line, const char* file, const char* err, ...) {
		va_list args = NULL;
		va_start(args, err);
		vprintf(err, args);
		printf(" (Line: %s | File: %s)\n", line, file);
		va_end(args);
	}

	#define error(...) error_(GFX_STR_MACRO(__LINE__), __FILE__, "2DGFX Internal Error: " __VA_ARGS__)
	#define info(...)  error_(GFX_STR_MACRO(__LINE__), __FILE__, "           2DGFX Log: " __VA_ARGS__)

	#define CHECK_CALL(call, errorfn, ...) if(call) { error(__VA_ARGS__); errorfn; }
	#define DEBUG_MODE(...) __VA_ARGS__;
	
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
	#define DEBUG_MODE(...) 
#endif


#if defined TRACY_ENABLE

	// https://luxeengine.com/integrating-tracy-profiler-in-cpp/
	// https://github.com/wolfpld/tracy/blob/master/public/tracy/TracyOpenGL.hpp
	// https://github.com/wolfpld/tracy/blob/cc3cbfe6f2a99d776e8f8642878cc16809bee078/public/client/TracyProfiler.cpp#L4311
	#include "../deps/extern/tracy/tracy/TracyC.h"
	
	#define PROFILER_CALLSTACK_DEPTH 7
	
	TracyCZoneCtx zone;
	// #define PROFILER_CALL(...) __VA_ARGS__;
	#define PROFILER_FRAME_MARK TracyCFrameMark;
	#define PROFILER_ZONE_START TracyCZoneS(tracy_ctx, PROFILER_CALLSTACK_DEPTH, 1)
	#define PROFILER_ZONE_END TracyCZoneEnd(tracy_ctx)

	#define PROFILER_QUERY_ID_COUNT 65536
	GLuint tracy_query_ids[PROFILER_QUERY_ID_COUNT];
	u32    tracy_current_query = 0;
	u32    tracy_last_submitted_query = 0;
	#define PROFILER_GPU_ZONE_START(zone_name) \
		glQueryCounter(tracy_query_ids[tracy_current_query], GL_TIMESTAMP);\
		___tracy_emit_gpu_zone_begin_alloc_callstack((const struct ___tracy_gpu_zone_begin_callstack_data) {\
			.srcloc = ___tracy_alloc_srcloc_name(__LINE__, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__) - 1, zone_name, sizeof(zone_name) - 1),\
			.depth = PROFILER_CALLSTACK_DEPTH,\
			.queryId = tracy_current_query,\
			.context = 1\
		});\
		tracy_current_query = (tracy_current_query + 1) % PROFILER_QUERY_ID_COUNT;
	#define PROFILER_GPU_ZONE_END() \
		glQueryCounter(tracy_query_ids[tracy_current_query], GL_TIMESTAMP);\
		___tracy_emit_gpu_zone_end((const struct ___tracy_gpu_zone_end_data) {\
			.queryId = tracy_current_query,\
			.context = 1\
		});\
		tracy_current_query = (tracy_current_query + 1) % PROFILER_QUERY_ID_COUNT;
	#define PROFILER_GPU_QUERIES_COLLECT() \
		while (tracy_last_submitted_query != tracy_current_query) {\
			GLint tracy_query_available = 0;\
			glGetQueryObjectiv(tracy_query_ids[tracy_last_submitted_query], GL_QUERY_RESULT_AVAILABLE, &tracy_query_available);\
			if(!tracy_query_available) continue;\
			u64 tracy_gpu_time;\
			glGetQueryObjectui64v(tracy_query_ids[tracy_last_submitted_query], GL_QUERY_RESULT, &tracy_gpu_time);\
			___tracy_emit_gpu_time((const struct ___tracy_gpu_time_data) {\
				.gpuTime = tracy_gpu_time,\
				.queryId = tracy_last_submitted_query,\
				.context = 1\
			});\
			tracy_last_submitted_query = (tracy_last_submitted_query + 1) % PROFILER_QUERY_ID_COUNT;\
		}
	#define PROFILER_GPU_INIT \
		glGenQueries(PROFILER_QUERY_ID_COUNT, tracy_query_ids);\
		i64 gputime;\
		glGetInteger64v(GL_TIMESTAMP, &gputime);\
		___tracy_emit_gpu_new_context((const struct ___tracy_gpu_new_context_data) {\
			.gpuTime = gputime,\
			.period = 1.0f,\
			.context = 1 /*idfk at this point*/,\
			.type = 1 /*Tracy::GpuContextType::OpenGL*/,\
			.flags = 0\
		});

	static inline void* tracy_malloc(size_t s) {
		void* ret = malloc(s);
		TracyCAlloc(ret, s)
		return ret;
	}
	static inline void* tracy_calloc(size_t n, size_t s) {
		void* ret = calloc(n, s);
		TracyCAlloc(ret, n * s);
		return ret;
	}
	static inline void* tracy_realloc(void* ptr, size_t s) {
		void* prev = ptr;
		void* ret = realloc(ptr, s);
		if(prev != ret) {
			TracyCFree(prev)
			TracyCAlloc(ret, s)
		}
		return ret;
	}
	static inline void tracy_free(void* ptr) {
		TracyCFree(ptr);
		free(ptr);
	}
	
	#define VEC_H_CALLOC tracy_calloc
	#define VEC_H_REALLOC tracy_realloc
	#define VEC_H_FREE tracy_free
	
	#define VEC_H_IMPLEMENTATION
	#include <vec.h>
	
	#define GFX_MALLOC tracy_malloc
	#define GFX_CALLOC tracy_calloc
	#define GFX_REALLOC tracy_realloc
	#define GFX_FREE tracy_free
#else
	#define VEC_H_IMPLEMENTATION
	#include <vec.h>
	// #define PROFILER_CALL(...) 
	#define PROFILER_FRAME_MARK 
	#define PROFILER_ZONE_START 
	#define PROFILER_ZONE_END 
	#define PROFILER_GPU_INIT 
	#define PROFILER_GPU_ZONE_START(name) 
	#define PROFILER_GPU_ZONE_END() 
	#define PROFILER_GPU_QUERIES_COLLECT() 
	#define GFX_CALLOC calloc
	#define GFX_MALLOC malloc
	#define GFX_REALLOC realloc
#endif


// -------------------------------------- Struct definitions --------------------------------------

typedef struct gfx_texture       gfx_texture;
typedef struct gfx_texture_atlas gfx_texture_atlas;
typedef struct gfx_atlas_node    gfx_atlas_node;
typedef struct gfx_atlas_added   gfx_atlas_added;
typedef struct gfx_typeface      gfx_typeface;
typedef struct gfx_drawbuf       gfx_drawbuf;
typedef struct gfx_vertexbuf     gfx_vertexbuf;
typedef struct gfx_gl_texture    gfx_gl_texture;
typedef struct gfx_char          gfx_char;
typedef struct gfx_char_ident    gfx_char_ident;

struct gfx_char {
	gfx_vector place;   // Place in texture atlas
	gfx_vector size;    // w, h of char texture
	gfx_vector bearing; // Refer to freetype docs on this, but I need to keep in track of it.
	f32 advance;        // How far this character moves forward the text cursor
	u32 atlas;          // Index of the atlas the character is located in
	f32 layer;          // Layer of the texture array that the character resides in
	unsigned long c;    // The character(unicode)
};

struct gfx_char_ident {
	FT_ULong c;
	u32 size;
};

struct gfx_drawbuf {
	struct gfx_vertexbuf {
		vec2 p;
		float z;
		vec2 tp;
		float index;
		vec4 col;
	}*   shp;
	u32* idx;
	u32 shpstart;
	u32 shpframeend;
};

/**
	Way drawing is done:
		- Goal: Reduce draw calls and state changes
		- Current method:
			- Push vertices onto a draw buffer per texture.
			- Sequentially draw textures/texture arrays
*/


struct gfx_ctx {
	GLFWwindow* window; // GLFW Window
	u32 width, height;
	vec4 curcol;        // Current color
	vec4 stroke;        // Current stroke
	u32 slots;          // Array of length 32 of booleans for slots(OpenGL only supports 32).
	float z;            // Current "z-index"
	
	struct gfx_texture_atlas {
		struct gfx_atlas_added {
			gfx_vector place;
			gfx_vector size;
		}* added;
		u32 gltex;
		u32 layers;
		gfx_vector size;
		struct gfx_atlas_node {
			gfx_vector p; // Place
			gfx_vector s; // Size
			u32 child[2];
			bool filled;
		}** atlas_trees; // Array of trees, one tree per layer
	}* atlases;
	
	struct gfx_texture {
		u32 gltex;
		gfx_vector size;
	}* textures;

	
	// OpenGL related variables.
	struct {
		GLuint progid, varrid, vbufid, idxbufid;
		GLuint curtex;
		GLint max_texture_layers;
		gfx_vector maxdrawbufsize;
		struct gfx_gl_texture {
			bool mipmaps;
			bool pixellated;
			bool array;
			GLuint id;
			GLuint slot;
			GLenum format;
			u32 uses;
			gfx_drawbuf drawbuf;
			u8* buf;
		}* gltexes;
		ht(char*, GLint) uniforms;
	} gl;

	struct {
		struct gfx_typeface {
			char* name;
			u32 space_width;
			FT_Face face;
			ht(gfx_char_ident, gfx_char) chars;
		}* store;
		u32 cur, size, lh;
	} fonts;

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
		u32 count;
	} frame;

	u32 curdrawbuf;

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
	TEXT = 0, SHAPE = 0, IMAGE = 1, SDF_TEXT = 3
};

static struct {
	const char* vert;
	const char* frag;
} default_shaders = {
	.vert = GLSL(
		// CGLSLSTART
		precision mediump float;
		layout (location = 0) in vec4 pos;
		layout (location = 1) in vec2 uv;
		layout (location = 2) in float idx;
		layout (location = 3) in vec4 col;

		uniform mat4 u_mvp;

		out vec2 v_uv;
		out vec4 v_col;
		out vec2 v_pos;
		out float v_idx;

		void main() {
			gl_Position = u_mvp * pos;
			// v_pos = pos;
			v_uv = uv;
			v_col = col;
			v_idx = idx;
		}
		// CGLSLEND
	),

	.frag = GLSL(
		// CGLSLSTART
		precision mediump float;
		layout (location = 0) out vec4 color;
		
		in vec2 v_uv;
		in vec4 v_col;
		in vec2 v_pos;
		in float v_idx;

		uniform sampler2D u_tex;
		uniform int u_shape;

		vec4 text;
		
		void main() {
			if(v_uv.x < 0) {
				color = v_col; // Shape rendering
				return;
			}
			text = texture(u_tex, v_uv);
			if(u_shape == 0) {
				color = vec4(v_col.rgb, text.r * v_col.a); // Text rendering
			}
			else {
				color = vec4(text.rgb, text.a * v_col.a); // Image rendering
			}
			
			if(color.a <= .1) {
				discard;
			}
		}
		// CGLSLEND
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
		case GL_RED:
			return 1;
		case GL_RGB:
		case GL_RGB8:
			return 3;
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

static inline GLuint gfx_compshader(GLenum type, const char* src) {

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
				message = GFX_MALLOC(length * sizeof(char));
				glGetShaderInfoLog(id, length, &length, message);
			}
			error("Failed to compile %s shader!\n%s\n", (type == GL_VERTEX_SHADER ? "vertex" : "fragment"), message);
			glDeleteShader(id); return 0;
		}
	#endif
	
	return id;
}


static inline GLuint gfx_shaderprog(const char* vert, const char* frag) {
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
static inline struct gfx_atlas_node* gfx_atlas_insert(u32 atlas, u32 layer, u32 parent, gfx_vector* size) {
	gfx_atlas_node* prnt = ctx->atlases[atlas].atlas_trees[layer] + parent;

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
		if (prnt->p.x + prnt->s.w == INT_MAX) realsize.w = ctx->atlases[atlas].size.w - prnt->p.x;
		if (prnt->p.y + prnt->s.h == INT_MAX) realsize.h = ctx->atlases[atlas].size.h - prnt->p.y;
		
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
		vpusharr(ctx->atlases[atlas].atlas_trees[layer], { c1, c2 }); // THIS MODIFIES THE POINTER SO YOU CANNOT USE PRNT BECAUSE IT USES THE OLD PTR
		ctx->atlases[atlas].atlas_trees[layer][parent].child[0] = vlen(ctx->atlases[atlas].atlas_trees[layer]) - 2;
		ctx->atlases[atlas].atlas_trees[layer][parent].child[1] = vlen(ctx->atlases[atlas].atlas_trees[layer]) - 1;
		
		return gfx_atlas_insert(atlas, layer, ctx->atlases[atlas].atlas_trees[layer][parent].child[0], size);
	}

	struct gfx_atlas_node* firstchildins = gfx_atlas_insert(atlas, layer, prnt->child[0], size);
	return firstchildins ? firstchildins : gfx_atlas_insert(atlas, layer, prnt->child[1], size);
}

// ------------------------------------------ Util Funcs ------------------------------------------

static inline u32 gfx_curidx(gfx_drawbuf* dbuf, u32 numverts) {
	if(dbuf->shpstart - numverts > 0) return dbuf->shpstart -= numverts;
	
	return vlen(dbuf->shp);
}
static inline void gfx_advance_z() { ctx->z += 1.0f / FLOAT_PRECION_BASED_MAX; }

// Aligns to the next multiple of a, where a is a power of 2
static inline u32 gfx_align(u32 n, u32 a) { return (n + a - 1) & ~(a - 1); }

static inline u32 gfx_totalarea(gfx_atlas_added* boxes /* Vector<gfx_atlas_added> */) {
	u32 total = 0;
	for(u32 i = 0; i < vlen(boxes); i ++)
		total += boxes[i].size.w * boxes[i].size.h;
	return total;
}

static size_t gfx_read(char* file, char** (*buf)) {
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
	
	*(*buf) = GFX_MALLOC(len * sizeof(char) + 1);
	*(*buf)[len] = '\0';
	fread((*buf), 1, len, fp);
	fclose(fp);

	// returns both
	return len;
}

static inline u8* gfx_readtoimg(char* file, int* width, int* height) {
	int channels;
	// stbi_set_flip_vertically_on_load(true);
	return stbi_load(file, width, height, &channels, STBI_rgb_alpha);
}

// ---------------------------------- OpenGL Uniforms Functions ----------------------------------

// Gets the location from cache or sets it in cache
static inline GLint gfx_uloc(char* name) {
	GLint* v = hgets(ctx->gl.uniforms, name);
	if(v == NULL) return hsets(ctx->gl.uniforms, name) = glGetUniformLocation(ctx->gl.progid, name);
	return *v;
}

static inline void gfx_usetm4(char* name, mat4x4 p) { glUniformMatrix4fv(gfx_uloc(name), 1, GL_FALSE, (const GLfloat*) p); }
static inline void gfx_useti(char* name, unsigned int p) { glUniform1i(gfx_uloc(name), p); }
static inline void gfx_uset3f(char* name, vec3 p) { glUniform3f(gfx_uloc(name), p[0], p[1], p[2]); }
static inline void gfx_uset2f(char* name, vec2 p) { glUniform2f(gfx_uloc(name), p[0], p[1]); }

// --------------------------------------- Input Functions ---------------------------------------

gfx_vector gfx_mouse() {
	double xpos, ypos;
	glfwGetCursorPos(ctx->window, &xpos, &ypos);
	return (gfx_vector) { xpos, ypos };
}

gfx_vector gfx_screen_dims() {
	return (gfx_vector) { .w = ctx->width, .h = ctx->height };
}

// Creates a projection in proportion to the screen coordinates
void gfx_updatescreencoords(u32 width, u32 height) {
	ctx->width = width; ctx->height = height;
	mat4x4_identity(ctx->transform.screen);
	mat4x4_ortho(ctx->transform.screen, .0f, (f32) width, (f32) height, .0f, -1.0f, 1.0f);
	gfx_usetm4("u_mvp", ctx->transform.screen);
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

// ----------------------------------------- GFX Headers -----------------------------------------


static inline u32 gfx_atlas_new(GLenum format);
static inline void drawtext();

// --------------------------------------- Setup Functions ---------------------------------------

FT_Library ft = NULL;


void gfx_setctx(struct gfx_ctx* c) {
	ctx = c;
	glBindVertexArray(c->gl.varrid);
	glUseProgram(c->gl.progid);
}

struct gfx_ctx* gfx_init(const char* title, u32 w, u32 h) {
	GLFWwindow* window;
	DEBUG_MODE(glfwSetErrorCallback(glfwErrorHandler))
	
	// Initializes the library
	CHECK_CALL(!glfwInit(), return NULL, "Couldn't initialize glfw3.");

	// GLFW hints
	// glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	// glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	DEBUG_MODE(glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE))

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
	
	PROFILER_GPU_INIT

	// Debug callback for detecting OpenGL errors
	DEBUG_MODE(glDebugMessageCallback(glDebugMessageHandler, NULL))

	// Enables OpenGL Features
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	
	// Fills application context struct
	struct gfx_ctx* ctx = GFX_CALLOC(1, sizeof(struct gfx_ctx));
	ctx->gl.progid = gfx_shaderprog(default_shaders.vert, default_shaders.frag);
	glGenVertexArrays(1, &ctx->gl.varrid);
	ctx->textures = vnew();
	ctx->atlases = vnew();
	ctx->gl.gltexes = vnew();
	ctx->fonts.store = vnew();
	ctx->fonts.size = 48;
	ctx->z = -1.0f;
	ctx->window = window;


	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &ctx->gl.max_texture_layers);
	info("Max texture layers allowed: %d", ctx->gl.max_texture_layers);

	// Initializes OpenGL Context
	gfx_setctx(ctx);
	
	// Text atlas
	gfx_atlas_new(GL_RED);
	gfx_updatescreencoords(w, h);
	fill(255, 255, 255, 255);
	return ctx;
}

bool gfx_nextframe() {
	PROFILER_FRAME_MARK
	PROFILER_ZONE_START
	glfwPollEvents();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ctx->frame.count ++;
	ctx->frame.start = glfwGetTime();
	ctx->frame.delta = ctx->frame.start - ctx->frame.last;
	ctx->frame.last  = ctx->frame.start;

	PROFILER_ZONE_END
	if (glfwWindowShouldClose(ctx->window)) {
		PROFILER_GPU_QUERIES_COLLECT()
		return false;
	} else return true;
}
void gfx_frameend() {
	PROFILER_ZONE_START
	PROFILER_GPU_ZONE_START("frameend")
	drawtext();
	ctx->z = -1.0f;
	glfwSwapBuffers(ctx->window);
	PROFILER_GPU_ZONE_END()
	if(ctx->frame.count % 500 == 0) {
		PROFILER_GPU_QUERIES_COLLECT()
	}
	PROFILER_ZONE_END
}

void gfx_close() {
	glfwSetWindowShouldClose(ctx->window, 1);
}

#define FPS_RECALC_DELTA .1
bool gfx_fpschanged() {
	return ctx->frame.start - ctx->frame.lastfpscalc >= FPS_RECALC_DELTA;
}

double gfx_fps() {
	double rn = ctx->frame.start;
	if (rn - ctx->frame.lastfpscalc < FPS_RECALC_DELTA ||
		ctx->frame.count - ctx->frame.lastfpscount == 0) return ctx->frame.lastfpsnum;
	ctx->frame.lastfpsnum = ((double) ctx->frame.count - ctx->frame.lastfpscount) / (rn - ctx->frame.lastfpscalc);
	ctx->frame.lastfpscount = ctx->frame.count;
	ctx->frame.lastfpscalc = rn;
	return ctx->frame.lastfpsnum;
}

















// -------------------------------------- Graphics Functions --------------------------------------

static inline void draw() {
	PROFILER_ZONE_START
	PROFILER_GPU_ZONE_START("draw")
	gfx_gl_texture* tex = ctx->gl.gltexes + ctx->curdrawbuf;
	gfx_drawbuf* dbuf = &tex->drawbuf;
	u32 slen = vlen(dbuf->shp);
	u32 ilen = vlen(dbuf->idx);

	if(!ctx->gl.vbufid) {
		glGenBuffers(1, &ctx->gl.vbufid);
		glBindBuffer(GL_ARRAY_BUFFER, ctx->gl.vbufid);
		glGenBuffers(1, &ctx->gl.idxbufid);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->gl.idxbufid);
		
		// Applies layout
		gfx_applylayout(4, (struct gfx_layoutelement[]) {
			{ GL_FLOAT, 3, false }, // X, Y, Z
			{ GL_FLOAT, 2, false }, // Texture X, Texture Y
			{ GL_FLOAT, 1, false }, // Texture Index in 2D Array Textures
			{ GL_FLOAT, 4, false }  // Color (RGBA)
		});
	}

	// Vertex buffer upload
	if(ctx->gl.maxdrawbufsize.w < slen)
		glBufferData(GL_ARRAY_BUFFER, slen * sizeof(*dbuf->shp), dbuf->shp, GL_DYNAMIC_DRAW),
		ctx->gl.maxdrawbufsize.w = slen;
	else glBufferSubData(GL_ARRAY_BUFFER, 0, slen * sizeof(*dbuf->shp), dbuf->shp);
	
	// Element array buffer upload
	if(ctx->gl.maxdrawbufsize.h < ilen)
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ilen * sizeof(*dbuf->idx), dbuf->idx, GL_DYNAMIC_DRAW),
		ctx->gl.maxdrawbufsize.h = ilen;
	else glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, ilen * sizeof(*dbuf->idx), dbuf->idx);

	// Draw call
	glDrawElements(GL_TRIANGLES, ilen, GL_UNSIGNED_INT, NULL);

	// Reset draw buffers and Z axis
	vempty(dbuf->shp);
	vempty(dbuf->idx);
	PROFILER_GPU_ZONE_END()
	PROFILER_ZONE_END
}



void fill(u8 r, u8 g, u8 b, u8 a) {
	ctx->curcol[0] = (f32) r / 255.f;
	ctx->curcol[1] = (f32) g / 255.f;
	ctx->curcol[2] = (f32) b / 255.f;
	ctx->curcol[3] = (f32) a / 255.f;
}

void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
	PROFILER_ZONE_START
	gfx_drawbuf* dbuf = &ctx->gl.gltexes[ctx->curdrawbuf].drawbuf;
	u32 curidx = vlen(dbuf->shp);
	gfx_advance_z();
	
	vpusharr(dbuf->idx, { curidx, curidx + 1, curidx + 2, curidx + 2, curidx, curidx + 3 });
	vpusharr(dbuf->shp, {
		{ .p = { (f32) x1, (f32) y1 }, .z = ctx->z, .tp = { -1.0f, -1.0f }, .index = 0.0f, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { (f32) x2, (f32) y2 }, .z = ctx->z, .tp = { -1.0f, -1.0f }, .index = 0.0f, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { (f32) x3, (f32) y3 }, .z = ctx->z, .tp = { -1.0f, -1.0f }, .index = 0.0f, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } },
		{ .p = { (f32) x4, (f32) y4 }, .z = ctx->z, .tp = { -1.0f, -1.0f }, .index = 0.0f, .col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] } }
	});
	PROFILER_ZONE_END
}

void rect(int x, int y, int w, int h) {
	quad(x, y, x + w, y, x + w, y + h, x, y + h);
}

void background(u8 r, u8 g, u8 b, u8 a) {
	float tmp[4] = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] };
	fill(r, g, b, a);
	rect(0, 0, ctx->width, ctx->height);
	ctx->curcol[0] = tmp[0];
	ctx->curcol[1] = tmp[1];
	ctx->curcol[2] = tmp[2];
	ctx->curcol[3] = tmp[3];
}


#define PI 3.14159265358979f
void circle(int x, int y, int r) {
	gfx_drawbuf* dbuf = &ctx->gl.gltexes[ctx->curdrawbuf].drawbuf;
	u32 curidx = vlen(dbuf->shp);
	float radius = r;
	gfx_advance_z();
	
	// https://github.com/grimfang4/sdl-gpu/blob/master/src/renderer_shapes_GL_common.inl#L79
	float dt = 0.625f / sqrtf(radius);
	u32 numsegments = 2.0f * PI / dt + 1.0f;
	if(numsegments < 16)
		numsegments = 16, dt = 2.0f*PI/15.0f;


	float cosdt = cosf(dt);
	float sindt = sinf(dt);
	float fx = x, fy = y;
	float dx = cosdt, dy = sindt * dx, tmpdx;
	u32 i = 2;

	// First triangle, beginning of second triangle
	vpusharr(dbuf->idx, { curidx, curidx + 1, curidx + 2 });
	vpusharr(dbuf->shp, {
		{ // Center
			.p = { fx, fy }, .z = ctx->z,
			.tp = { -1.0f, -1.0f }, .index = 0.0f,
			.col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }
		},
		{ // First edge point
			.p = { fx + radius, fy }, .z = ctx->z,
			.tp = { -1.0f, -1.0f }, .index = 0.0f,
			.col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }
		}
	});

	while (i < numsegments) {
		vpush(dbuf->shp, {
			.p = { fx + radius * dx, fy + radius * dy }, .z = ctx->z,
			.tp = { -1.0f, -1.0f }, .index = 0.0f,
			.col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }
		});
		vpusharr(dbuf->idx, { curidx, curidx + i, curidx + i + 1 });
		
		tmpdx = cosdt * dx - sindt * dy;
		dy = sindt * dx + cosdt * dy;
		dx = tmpdx;
		i++;
	}

	// Last vertex
	vpush(dbuf->shp, {
		.p = { fx + radius * dx, fy + radius * dy }, .z = ctx->z,
		.tp = { -1.0f, -1.0f }, .index = 0.0f,
		.col = { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }
	});

	// Last triangle, middle, last point, first point
	vpusharr(dbuf->idx, { curidx, curidx + i, curidx + 1 });
}

void ellipse(int x, int y, int rx, int ry) {
	
}













// ------------------------------ Texture + Atlas Creation Functions ------------------------------

static inline u32 gfx_tex_new(GLenum format, u8* tex, u32 width, u32 height) {
	vpush(ctx->gl.gltexes, {
		.mipmaps = true,
		.array = false,
		.pixellated = false,
		.id = 0,
		.slot = -1,
		.uses = 0,
		.format = format,
		.drawbuf = {
			.shp = vnew(),
			.idx = vnew()
		},
		.buf = tex
	});
	vpush(ctx->textures, {
		.gltex = vlen(ctx->gl.gltexes) - 1,
		.size = { width, height },
	});
	return vlen(ctx->textures) - 1;
}


static inline u32 gfx_atlas_new(GLenum format) {
	gfx_atlas_node** atlas_trees = vnew();
	gfx_atlas_node* tree = vnew();
	vpush(tree, { .s = { INT_MAX, INT_MAX } });
	vpush(atlas_trees, tree);
	
	vpush(ctx->gl.gltexes, {
		.mipmaps = true,
		.array = true,
		.pixellated = true,
		.id = 0,
		.slot = -1,
		.uses = 0,
		.format = format,
		.drawbuf = {
			.shp = vnew(),
			.idx = vnew()
		},
		.buf = GFX_MALLOC(GFX_ATLAS_START_SIZE * GFX_ATLAS_START_SIZE * gfx_glsizeof(format))
	});
	vpush(ctx->atlases, {
		.added = vnew(),
		.layers = 1,
		.gltex = vlen(ctx->gl.gltexes) - 1,
		.size = { GFX_ATLAS_START_SIZE, GFX_ATLAS_START_SIZE },
		.atlas_trees = atlas_trees,
	});
	return vlen(ctx->atlases) - 1;
}

static inline u32 gfx_atlas_try_push(u32 atlas, u32 layer, struct gfx_atlas_node** maybe, gfx_vector* size) {
	gfx_texture_atlas* tex_atlas = ctx->atlases + atlas;
	u32 growth = 1;
	do {
		*maybe = gfx_atlas_insert(atlas, layer, 0, size);
		
		if (*maybe) return growth;
		else if (tex_atlas->size.w >= GFX_ATLAS_MAX_SIZE)
			return 0;
		
		info("Atlas (id: %d) size doubled", atlas);
		growth *= 2, tex_atlas->size.w *= 2, tex_atlas->size.h *= 2;
	} while(true);
}

/* @param pos: Supply a pointer to a vector in the overarching scope so we can write position to it */
static inline u32 gfx_atlases_add(GLenum format, gfx_vector* size, gfx_vector* pos, u32* ret_atlas) {
	u32 growth, cur_atlas = 0, cur_layer = 0;
	struct gfx_atlas_node* maybe;
	for(; cur_atlas < vlen(ctx->atlases); cur_atlas ++)
		if(ctx->gl.gltexes[ctx->atlases[cur_atlas].gltex].format == format)
			for(; cur_layer < vlen(ctx->atlases[cur_atlas].atlas_trees) &&
						!(growth = gfx_atlas_try_push(cur_atlas, cur_layer, &maybe, size)); cur_layer ++);
	
	// Positions the atlas to the correct one, as it was incremented once more than needed when the for loop was ending (previous bug 3:)
	cur_atlas --;

	if(growth == 0) {
		if(vlen(ctx->atlases[cur_atlas].atlas_trees) >= ctx->gl.max_texture_layers) {
			gfx_atlas_try_push(cur_atlas = gfx_atlas_new(format), (cur_layer = 0), &maybe, size);
			info("Generating new atlas (#%d)", vlen(ctx->atlases));
		} else {
			gfx_atlas_node* new_tree = vnew();
			u32 atlas_trees_last_element = vlen(ctx->atlases[cur_atlas].atlas_trees);
			vpush(new_tree, { .s = { INT_MAX, INT_MAX } });
			vpush(ctx->atlases[cur_atlas].atlas_trees, new_tree);
			gfx_atlas_try_push(cur_atlas, atlas_trees_last_element, &maybe, size);
			info("Pushing new layer to atlas %d", cur_atlas);
		}
	}

	gfx_texture_atlas* atlas = ctx->atlases + cur_atlas;
	gfx_gl_texture* tex = ctx->gl.gltexes + ctx->atlases[cur_atlas].gltex;

	if(growth > 1) {
		int ow = atlas->size.w / growth, oh = atlas->size.h / growth;
		u8* ntex = GFX_MALLOC(atlas->size.w * atlas->size.h * sizeof(u8));
		u8* otex = tex->buf;
		
		for(int i = 0; i < oh; i ++)
			memcpy(ntex + i * atlas->size.w, otex + i * ow, ow);

		tex->buf = ntex;
		free(otex);
	}

	vpush(atlas->added, { maybe->p, maybe->s });
	*pos = maybe->p;
	*ret_atlas = cur_atlas;
	return cur_layer;
}
















// --------------------------- OpenGL Texture Drawing Related Functions ---------------------------

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
		static inline const int MultiplyDeBruijnBitPosition[32] = 
		{
			0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
			31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
		};
		#define firstbit(c) MultiplyDeBruijnBitPosition[((u32)((c & -c) * 0x077CB531U)) >> 27];
	#endif
#endif

static inline u32 findslot() {
	u32 slot = firstbit(~ctx->slots) - 1;
	if(slot < 0) {
		for(int i = 0; i < vlen(ctx->gl.gltexes); i ++)
			if(ctx->gl.gltexes[i].slot == 31) ctx->gl.gltexes[i].slot = -1;
		return 31;
	}
	ctx->slots |= 1 << slot;
	return slot;
}


// Sets texture lookup coordinates
static inline void gfx_tp(f32* texcoords) {
	struct gfx_vertexbuf* last4 = vlast(ctx->gl.gltexes[ctx->curdrawbuf].drawbuf.shp) - 3;
	for(int i = 0; i < 4; i ++)
		last4[i].tp[0] = texcoords[i * 2],
		last4[i].tp[1] = texcoords[i * 2 + 1];
}

static inline void gfx_usetex(u32 gltex) {
	ctx->curdrawbuf = gltex;
	ctx->gl.gltexes[gltex].uses++;
}

static inline void gfx_setcurtex(u32 gltex) {
	gfx_usetex(gltex);
	if(ctx->gl.curtex == ctx->gl.gltexes[gltex].slot) return;
	gfx_useti("u_tex", (ctx->gl.curtex = ctx->gl.gltexes[gltex].slot));
}

static inline void gfx_new_tex_slot(u32 gltex) {
	glActiveTexture(GL_TEXTURE0 + (ctx->gl.gltexes[gltex].slot = findslot()));
}

static inline void gfx_tex_upload(u32 gltex, gfx_vector size, u32 layers) {
	PROFILER_GPU_ZONE_START("texupload")
	gfx_new_tex_slot(gltex);
	gfx_gl_texture* t = ctx->gl.gltexes + gltex;
	glPixelStorei(GL_UNPACK_ALIGNMENT, gfx_glsizeof(t->format));

	
	// Generates and binds the texture
	GLenum texture_type = layers <= 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY;
	glGenTextures(1, &t->id);
	glBindTexture(texture_type, t->id);
	
	// Sets default params
	GLenum minmagfilter = t->pixellated ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(texture_type, GL_TEXTURE_MIN_FILTER, minmagfilter);
	glTexParameteri(texture_type, GL_TEXTURE_MAG_FILTER, minmagfilter);
	glTexParameteri(texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Sends texture data to the gpu
	if(layers <= 1)
		glTexImage2D(texture_type, 0, t->format, size.w, size.h, 0, t->format, GL_UNSIGNED_BYTE, t->buf);
	else glTexImage3D(texture_type, 0, t->format, size.w, size.h, layers, 0, t->format, GL_UNSIGNED_BYTE, t->buf);
	
	// We just don't need mipmaps for fonts so we do this for normal images
	if(!t->pixellated) {
		glTexParameteri(texture_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glGenerateMipmap(texture_type); // CALL AFTER UPLOAD
	}
	PROFILER_GPU_ZONE_END()
}

static inline void gfx_tex_update(u32 gltex, gfx_vector size, u32 layers) {
	PROFILER_GPU_ZONE_START("texupdate")
	gfx_gl_texture* t = ctx->gl.gltexes + gltex;
	
	if(t->slot < 0) gfx_new_tex_slot(gltex);
	else glActiveTexture(GL_TEXTURE0 + t->slot);
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, gfx_glsizeof(t->format));
	if(layers <= 1)
		glTexImage2D(GL_TEXTURE_2D, 0, t->format, size.w, size.h, 0, t->format, GL_UNSIGNED_BYTE, t->buf);
	else glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, t->format, size.w, size.h, layers, 0, t->format, GL_UNSIGNED_BYTE, t->buf);
	PROFILER_GPU_ZONE_END()
}

img gfx_loadimg(char* file) {
	int width, height;
	u8* t; CHECK_CALL(!(t = gfx_readtoimg(file, &width, &height)), return -1, "Couldn't load image '%s'", file);
	info("Loaded image '%s' (%dx%d)", file, width, height);
	return gfx_tex_new(GL_RGBA, t, width, height);
}


void image(img img, int x, int y, int w, int h) {
	PROFILER_ZONE_START
	if(img < 0) return;
	gfx_gl_texture* tex = ctx->gl.gltexes + ctx->textures[img].gltex;
	
	// Sets up the environment
	if(!tex->id) gfx_tex_upload(ctx->textures[img].gltex, ctx->textures[img].size, 1);
	else if(tex->slot < 0) gfx_new_tex_slot(ctx->textures[img].gltex);
	gfx_usetex(ctx->textures[img].gltex);

	// Creates a rectangle the image will be held on, then uploads the texture coordinates to map to the image
	rect(x, y, w, h);
	gfx_tp((float[]) { 0.f, 0.f, 1.0f, 0.f, 1.0f, 1.0f, 0.f, 1.0f });
	PROFILER_ZONE_END
}

gfx_vector* isize(img img) { return &ctx->textures[img].size; }










// ------------------------------------ Text Drawing Functions ------------------------------------

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
	PROFILER_ZONE_START
	gfx_typeface* face = ctx->fonts.store + tf;
	CHECK_CALL(FT_Set_Pixel_Sizes(face->face, 0, ctx->fonts.size * 4.0f / 3.0f), return NULL, "Couldn't set size");
	CHECK_CALL(FT_Load_Char(face->face, c, FT_LOAD_RENDER), return NULL, "Couldn't load char '%c' (%X)", c, c);
	
	int width = face->face->glyph->bitmap.width;
	int height = face->face->glyph->bitmap.rows;

	// Tries to add the character, resizing the whole texture until it's done
	gfx_vector pos;
	gfx_vector size = { width, height };
	u32 atlas;
	u32 layer = gfx_atlases_add(GL_RED, &size, &pos, &atlas);
	gfx_gl_texture* tex = ctx->gl.gltexes + ctx->atlases[atlas].gltex;

	// Writes the character's pixels to the atlas buffer.
	u8* insertsurface = tex->buf + pos.x + pos.y * ctx->atlases[atlas].size.w;
	for(int i = 0; i < height; i ++)
		memcpy(insertsurface + i * ctx->atlases[atlas].size.w, face->face->glyph->bitmap.buffer + i * width, width);

	hsetst(face->chars, { c, ctx->fonts.size }) = (gfx_char) {
		.c = c,
		.size = { .fx = (float) size.x, .fy = (float) size.y },
		.bearing = (gfx_vector) {
			.fx = (float) face->face->glyph->bitmap_left,
			.fy = (float) face->face->glyph->bitmap_top
		},
		.advance = (float) (face->face->glyph->advance.x >> 6),
		.place = { .fx = pos.x, .fy = pos.y },
		.atlas = atlas,
		.layer = (float) layer
	};
	PROFILER_ZONE_END
	return hlastv(face->chars);
}

static char* default_fallbacks[] = {
	
};

typeface gfx_loadfont(const char* file) {
	PROFILER_ZONE_START
	gfx_typeface new = {
		.name = GFX_MALLOC(strlen(file) + 1),
		.chars = {0},
		.face = NULL
	};
	memcpy(new.name, file, strlen(file) + 1);
	
	// Loads the freetype library.
	if(!ft)
		CHECK_CALL(FT_Init_FreeType(&ft), return -1, "Couldn't initialize freetype");

	// Loads the new face in using the library
	CHECK_CALL(FT_New_Face(ft, file, 0, (FT_Face*) &new.face), return -1, "Couldn't load font '%s'", file);
	CHECK_CALL(FT_Set_Pixel_Sizes(new.face, 0, RENDERING_FONT_SIZE()), return -1, "Couldn't set size");

	// Adds space_width
	CHECK_CALL(FT_Load_Char(new.face, ' ', FT_LOAD_RENDER), return -1, "Couldn't load the Space Character ( ) ");
	new.space_width = new.face->glyph->advance.x >> 6;

	// Stores the font
	vpush(ctx->fonts.store, new);
	info("Loaded font '%s'", new.name);
	PROFILER_ZONE_END
	return vlen(ctx->fonts.store) - 1;
}

void fontsize(u32 size) { ctx->fonts.size = size; }
void lineheight(f32 h) { ctx->fonts.lh = h; }

// Draws any leftover shapes and text
static inline void drawtext() {
	PROFILER_ZONE_START
	PROFILER_GPU_ZONE_START("drawtext")
	for(u32 i = 0; i < vlen(ctx->atlases); i ++) {
		gfx_texture_atlas* atlas = ctx->atlases + i;
		gfx_gl_texture* gtex = ctx->gl.gltexes + atlas->gltex;
		if(!vlen(gtex->drawbuf.idx)) continue;
		if(!gtex->id)
			gfx_tex_upload(atlas->gltex, atlas->size, atlas->layers);
		else if (vlen(atlas->added)) {
			if(vlen(atlas->added) < 200 && gfx_totalarea(atlas->added) < atlas->size.w * atlas->size.h / 5) {
				if(gtex->slot < 0) gfx_new_tex_slot(atlas->gltex);
				else glActiveTexture(GL_TEXTURE0 + gtex->slot);

				gfx_atlas_added* added = atlas->added;
				u32 maxbufsize = added->size.w * added->size.h;
				u8* buf = GFX_MALLOC(maxbufsize * gfx_glsizeof(gtex->format));
				
				for(u32 i = 0; i < vlen(added); i ++) {
					if(added[i].size.w * added[i].size.h > maxbufsize) {
						maxbufsize = added[i].size.w * added[i].size.h;
						buf = GFX_REALLOC(buf, maxbufsize * gfx_glsizeof(gtex->format));
					}

					u8* srcbuf = gtex->buf + atlas->size.w * added[i].place.y + added[i].place.x;
					for(u32 j = 0; j < added[i].size.h; j ++)
						memcpy(buf + j * added[i].size.w, srcbuf + atlas->size.w * j, added[i].size.w);

					glTexSubImage2D(GL_TEXTURE_2D, 0, added[i].place.x, added[i].place.y, added[i].size.w, added[i].size.h, gtex->format, GL_UNSIGNED_BYTE, buf);
				}
				free(buf);
			}
			else gfx_tex_update(atlas->gltex, atlas->size, atlas->layers);
			stbi_write_png("bitmap.png", atlas->size.w, atlas->size.h, 1, gtex->buf, atlas->size.w);
			vempty(atlas->added);
		} else if(gtex->slot < 0) gfx_new_tex_slot(atlas->gltex);
		gfx_setcurtex(atlas->gltex);

		if(gtex->format == GL_RED)
			gfx_useti("u_shape", TEXT);
		else gfx_useti("u_shape", IMAGE);
		
		draw();
	}

	for(u32 i = 0; i < vlen(ctx->textures); i ++) {
		gfx_gl_texture* gtex = ctx->gl.gltexes + ctx->textures[i].gltex;
		if(!vlen(gtex->drawbuf.idx)) continue;
		if(!gtex->id)
			gfx_tex_upload(ctx->textures[i].gltex, ctx->textures[i].size, 1);
		else if(gtex->slot < 0) gfx_new_tex_slot(ctx->textures[i].gltex);
		gfx_setcurtex(ctx->textures[i].gltex);

		gfx_useti("u_shape", IMAGE);
		draw();
	}
	PROFILER_GPU_ZONE_END()
	PROFILER_ZONE_END
}
void font(typeface face, u32 size) {
	
// 	// If you're setting to the same font, just return preemptively
// 	if(face == ctx->fonts.cur) goto end;
	
// 	// Draws anything left in the shape buffer from previous fonts
// 	if(vlen(ctx->drawbuf.shp) && ctx->drawbuf.textdrawn) drawtext();
	
// 	ctx->fonts.cur = face;
// 	ctx->drawbuf.textdrawn = false;
// end:
// 	if(size > 0) ctx->fonts.size = size;
}
void text(const char* str, int x, int y) {
	PROFILER_ZONE_START
	// PROFILER_CALL(TracyCZoneText(tracy_ctx, str, strlen(str)))
	if(!vlen(ctx->fonts.store)) return;

	gfx_typeface* face = ctx->fonts.store + ctx->fonts.cur;
	f32 scale = (f32) ctx->fonts.size * 4.0f / 3.0f /*px -> pts*/ / RENDERING_FONT_SIZE(.0f);
	gfx_advance_z();

	FT_ULong point;
	f32 curx = x, cury = y; // So we don't have to do so many useless conversions! (profiled - performance bottleneck)
	f32 tx, ty, tw, th, realx, realy, w, h;
	while ((point = gfx_readutf8((u8**) &str))) {

		// Newlines in five lines :D
		if (point == '\n') {
			cury += ctx->fonts.lh * ctx->fonts.size * 4.0f / 3.0f;
			curx = x;
			continue;
		}

		else if(point == ' ') {
			curx += face->space_width * scale;
			continue;
		}
		
		gfx_char* ch = hgetst(face->chars, { point, ctx->fonts.size });
		if(!ch) ch = gfx_loadfontchar(ctx->fonts.cur, point);
		if(!ch) continue;
		
		gfx_texture_atlas* atlas = ctx->atlases + ch->atlas;
		gfx_drawbuf* dbuf = &ctx->gl.gltexes[atlas->gltex].drawbuf;

		realx = curx + ch->bearing.fx;
		realy = cury - ch->bearing.fy;
		w     = ch->size.fx;
		h     = ch->size.fy;

		tx = ch->place.fx / (f32) atlas->size.w;
		ty = ch->place.fy / (f32) atlas->size.h;
		tw = ch->size.fx  / (f32) atlas->size.w;
		th = ch->size.fy  / (f32) atlas->size.h;
		
		// Kind of copied from quad() so it can be more efficient with less int -> float -> int conversions.
		u32 curidx = vlen(dbuf->shp);
		vpusharr(dbuf->idx, { curidx, curidx + 1, curidx + 2, curidx + 2, curidx, curidx + 3 });
		vpusharr(dbuf->shp, {
			{{ realx    , realy     }, ctx->z, { tx     , ty      }, ch->layer, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ realx + w, realy     }, ctx->z, { tx + tw, ty      }, ch->layer, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ realx + w, realy + h }, ctx->z, { tx + tw, ty + th }, ch->layer, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }},
			{{ realx    , realy + h }, ctx->z, { tx     , ty + th }, ch->layer, { ctx->curcol[0], ctx->curcol[1], ctx->curcol[2], ctx->curcol[3] }}
		});

		// Advance cursors for next glyph
		curx += ch->advance;
	}
	PROFILER_ZONE_END
}


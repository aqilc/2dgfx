/**
Current Tasks:
	- Change from using vertex attributes to using ~~uniform buffers~~ texture buffers.
		- Update shaders
		- Bind uniform blocks
		- Add Uniform Buffer pushing and modification.
		- Split up draw calls based on number of vertices/indices.
	- Later on, make it so that gfx_error pulls up a native window with the error, instead of doing nothing but crashing on release mode
*/
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

	SHADERS:
		Fast Gaussian Blur: https://github.com/Experience-Monks/glsl-fast-gaussian-blur

	DRAWING WITHOUT A WINDOW FOR OFFSCREEN GPU ACCELERATION:
		https://community.khronos.org/t/offscreen-rendering-without-a-window/57842/2
		https://stackoverflow.com/questions/2896879/windowless-opengl
		https://stackoverflow.com/a/48947103/10013227

	DEBUGGING RESOURCES:
		https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/getting-started-with-windbg#summary-of-commands
		https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/commands
		https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/application-verifier
		https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/application-verifier-testing-applications
		https://stackoverflow.com/questions/47711390/address-sanitizer-like-functionality-on-msvc

	VSYNC AND FRAMERATE CONTROL:
		Fix your Timestep: https://gafferongames.com/post/fix_your_timestep/
		https://github.com/dvdhrm/docs/blob/master/drm-howto/modeset-vsync.c

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
			- [x] font_size()
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
			- https://github.com/protectwise/troika/tree/main/packages/troika-three-text
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
#define HASH_H_CUSTOM_HASHER
#include <hash.h>

#define XXH_STATIC_LINKING_ONLY   /* access advanced declarations */
#define XXH_INLINE_ALL
#define XXH_NO_XXH3
#define XXH_IMPLEMENTATION   /* access definitions */
#include <xxHash/xxhash.h>

#include <math.h>
#include <linmath.h>

#include "2dgfx.h"



/*
	audio impl: (easy) https://github.com/zserge/fenster/blob/main/fenster_audio.h
*/



#define GFX_STR(...) #__VA_ARGS__
#define GFX_STR_MACRO(...) GFX_STR(__VA_ARGS__)
#define GLSL(...) "#version 330 core\n" GFX_STR_MACRO(__VA_ARGS__)

#define FLOAT_PRECION_BASED_MAX 16777215.0f

#define UV_X_MAX 16383
#define UV_Y_MAX 8191

// Batch rendering macros
#define GFX_ATLAS_START_SIZE 512
#define GFX_ATLAS_MAX_GROWTH_FACTOR 4
#define GFX_ATLAS_MAX_SIZE (GFX_ATLAS_START_SIZE * GFX_ATLAS_MAX_GROWTH_FACTOR)
#define GFX_ATLAS_W(atlas) (GFX_ATLAS_START_SIZE * atlas->growth_factor * gfx_glsizeof(atlas->format))
#define RENDERING_FONT_SIZE(...) 48##__VA_ARGS__

#define GFX_DRAW_BUFFER_SIZE_INCREMENT (32768 / sizeof(gfx_vertexbuf))


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




#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
	// http://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FTime%2FNtSetTimerResolution.html
	// https://stackoverflow.com/a/22865850/10013227
	typedef LONG NTSTATUS;
	NTSYSAPI NTSTATUS NTAPI NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
	#ifndef __mingw32__
		#pragma comment(lib, "ntdll.lib")
		#pragma comment(lib, "user32.lib")
		#pragma comment(lib, "gdi32.lib")
		#pragma comment(lib, "shell32.lib")
		#pragma comment(lib, "opengl32.lib")
		#pragma comment(lib, "advapi32.lib")
	#endif
	#undef TEXT
#else
	#include <unistd.h>
#endif

#define GFX_DEBUG
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
	#define gfx_assert(expr, ...) if(!(expr)) { error("Assertion '" #expr "' Failed: " __VA_ARGS__); exit(1); }

	#define CHECK_CALL(call, errorfn, ...) if(call) { error(__VA_ARGS__); errorfn; }
	#define DEBUG_MODE(...) __VA_ARGS__;

	void GLAPIENTRY glDebugMessageHandler(GLenum source,
			GLenum type, GLuint id, GLenum severity, GLsizei length,
			const GLchar* message, const void* userParam) {
		printf("        OpenGL Error: %s (Code: %d)\n", message, type);
	}

	void glfwErrorHandler(int error, const char* desc) {
		printf("          GLFW Error: %s", desc);
	}

#else
	#define error(...)
	#define info(...)
	#define gfx_assert(expr, ...)
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

	#define PROFILER_QUERY_ID_COUNT 6553//6
	GLuint tracy_query_ids[PROFILER_QUERY_ID_COUNT];
	u32    tracy_current_query = 0;
	u32    tracy_last_submitted_query = 0;
	#define PROFILER_GPU_ZONE_START(zone_name) \
		glQueryCounter(tracy_query_ids[tracy_current_query], GL_TIMESTAMP);\
		___tracy_emit_gpu_zone_begin_alloc_callstack((const struct ___tracy_gpu_zone_begin_callstack_data) {\
			.srcloc = ___tracy_alloc_srcloc_name(__LINE__, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__) - 1, zone_name, sizeof(zone_name) - 1, 0),\
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
			if(!tracy_query_available) break;\
			u64 tracy_gpu_time;\
			glGetQueryObjectui64v(tracy_query_ids[tracy_last_submitted_query], GL_QUERY_RESULT, &tracy_gpu_time);\
			___tracy_emit_gpu_time((const struct ___tracy_gpu_time_data) {\
				.gpuTime = tracy_gpu_time,\
				.queryId = tracy_last_submitted_query,\
				.context = 1\
			});\
			tracy_last_submitted_query = (tracy_last_submitted_query + 1) % PROFILER_QUERY_ID_COUNT;\
		}
	#define PROFILER_GPU_INIT() \
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
		TracyCAlloc(ret, s);
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
			TracyCFree(prev);
			TracyCAlloc(ret, s);
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
	#define PROFILER_GPU_INIT()
	#define PROFILER_GPU_ZONE_START(name)
	#define PROFILER_GPU_ZONE_END()
	#define PROFILER_GPU_QUERIES_COLLECT()
	#define GFX_CALLOC calloc
	#define GFX_MALLOC malloc
	#define GFX_REALLOC realloc
#endif


// -------------------------------------- Struct definitions -------------------------------------- //

typedef struct gfx_texture        gfx_texture;
typedef struct gfx_atlas          gfx_atlas;
typedef struct gfx_internal_image gfx_internal_image;
typedef struct gfx_atlas_node     gfx_atlas_node;
typedef struct gfx_atlas_added    gfx_atlas_added;
typedef struct gfx_typeface       gfx_typeface;
typedef struct gfx_drawbuf        gfx_drawbuf;
typedef struct gfx_vtx_buf        gfx_vtx_buf;
typedef struct gfx_char           gfx_char;
typedef union  gfx_char_ident     gfx_char_ident;
typedef union  gfx_color          gfx_color;

typedef u32 gfx_tex_id;
typedef u32 gfx_atlas_id;
typedef u32 gfx_tex_hnd;
typedef u32 gfx_atlas_hnd;
typedef u8  gfx_slot_hnd;

///// IMPORTANT DISTINCTION /////
// IDs are the exact index, they are always existent and defined. An ID of 0 means that you should access the 0th element.
// HNDs are the opposite, they are +1 to the index. An HND of 0 means that the element does not exist. These are built as optionals.

enum gfx_drawprim {
	TEXT = 0, SHAPE = 1, IMAGE = 2, SDF_TEXT = 3
};
enum gfx_drawobj {
	OBJ_TEXT = 0, OBJ_QUAD = 1, OBJ_IMG = 2,
	OBJ_ELLIPSE = 3, OBJ_TRI = 4
};

struct gfx_char {
	gfx_vector_mini place;   // Place in texture atlas
	gfx_vector_mini size;    // w, h of char texture
	gfx_vector_mini bearing; // Refer to freetype docs on this, but I need to keep in track of it.
	u16 advance;             // How far this character moves forward the text cursor
	gfx_atlas_hnd atlas;     // Index of the atlas the character is located in
	u32 c;                   // The character(unicode)
};

union gfx_char_ident {
	struct {
		FT_ULong c;
		u32 size;
	};
	u64 full;
};
#define GFX_CHAR_EQUAL(a, b) (a.full == b.full)
#define GFX_CHAR_HASH(a) (ht_int64_hash_func(a.full))


// To draw a shape:
// Need to set:
// - x to x position
// - y to y position
// - type to either GFX_FULL, GFX_INNER, GFX_OUTER, GFX_TEX
// - set either .col or .tex_id, uv_x and uv_y.

// currently not really going to put effort into supporting gcc:
// https://www.reddit.com/r/C_Programming/comments/x5rxu3/interesting_gcc_and_clang_incompatibility/

struct gfx_drawbuf {
	#pragma pack(push, 1)
	struct gfx_vtx_buf {
		struct {
			int x  : 16;
			enum gfx_vtx_type: u32 { // is unsigned because I'm getting warnings that it's truncating 3 to -1 :skull:
			  GFX_FULL, GFX_INNER, GFX_OUTER, GFX_TEX
			} type : 2;
			int y  : 14;
		};
		union {
		  struct {
				unsigned int uv_y  : 13;
				unsigned int uv_x  : 14;
				unsigned int tex_slot: 5;
			};
  		union gfx_color {
  			GLuint full;
  			struct { GLubyte r, g, b, a; };
  			GLubyte bytes[4];
  		} col;
		};
	}* shp;
	#pragma pack(pop)

	u32* idx;

	struct gfx_uniformbuf {
		gfx_vector_mini pos;
		gfx_vector_mini size;
		gfx_vector_mini uv;
		GLshort stroke;
		GLubyte radius;
		GLubyte tex_id;
	}* extra;

	union {
		struct {
			enum gfx_drawobj type: 3;
			u32 hash: 29;
		};
		u32 full;
	}* hashes;
	gfx_vector maxdrawbufsize;
};

struct gfx_ctx {
	GLFWwindow* window; // GLFW Window
	u32 width, height;
	gfx_color curcol;   // Current color
	vec4 stroke;        // Current stroke
	gfx_settings settings;
#ifdef _WIN32
	HKEY key;
#endif

	struct gfx_atlas {
		struct gfx_atlas_added {
			gfx_vector_mini place;
			gfx_vector_mini size;
		}* added;
		u8* buf;
		u8 growth_factor;
		bool uploaded;
		u16 format;
		u32 tex_id;
		struct gfx_atlas_node {
			gfx_vector_mini p; // Place
			gfx_vector_mini s; // Size
			u32 child[2];
			bool filled;
		}* tree; // Array of trees, one tree per layer
	}* atlases;

	struct gfx_internal_image {
		gfx_atlas_hnd atlas_hnd; // -1
		gfx_tex_hnd tex_hnd;
		gfx_vector_mini size;
		gfx_vector_mini place;
	}* images;

	struct gfx_texture {
		GLuint id;
		gfx_slot_hnd slot;
	}* textures;

	// OpenGL related variables.
	struct {
		GLuint progid, varrid, vbufid, idxbufid;
		GLuint curtex;
		gfx_drawbuf drawbuf;
		ht(gfx_uni, char*, GLint) uniforms;
		gfx_tex_hnd slots[32];
		gfx_slot_hnd slot_bound;
	} gl;

	struct {
		struct gfx_typeface {
			const char* name;
			FT_Face face;
			u32 space_width;
			gfx_face fallback;
			ht(gfx_char, gfx_char_ident, gfx_char) chars;
		}* store;
		gfx_face cur;
		u32 size, lh;
	} font;

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

	struct {
		mat4x4 screen;
		mat4x4 cur;
		bool changed;
		bool used;
	} transform;
};

static _Thread_local struct gfx_ctx* ctx = NULL;

ht_impl(gfx_char, gfx_char_ident, gfx_char, GFX_CHAR_HASH, GFX_CHAR_EQUAL);
ht_impl_str(gfx_uni, GLint);

// -------------------------------- OpenGL Helper Functions + Data -------------------------------- //

#define TEXT 0
#define SHAPE 1
#define IMAGE 2
#define SDF_TEXT 3

static struct {
	const char* vert;
	const char* frag;
} default_shaders = {
	.vert = GLSL(
		// CGLSLSTART
		precision mediump float;
		layout (location = 0) in ivec2 pos;
		layout (location = 1) in vec4  col;
		layout (location = 1) in uvec4 info;

		uniform vec2 u_screen;

		out vec2 v_uv;
		out vec4 v_col;
		flat out uvec4 v_info;
		flat out uvec2 debug_uv;
		flat out uint v_type;
		flat out uint v_tex_id;
		// out vec2 v_pos;
		// out float v_idx;

		vec2 data[3] = vec2[3](
		  vec2(0.0, 0.0),
			vec2(0.5, 0.0),
			vec2(1.0, 1.0)
		);

		void main() {
		  uint type = uint((pos.y & int(0x3)));
			int y = (-pos.y) >> 2;

			if (type == uint(3)) {
			  uint tex_id = (info.w & uint(0xF8)) >> 3;
				uint uv_x = (((info.w << 11) & uint(0x3800)) | (info.z << 3) | (info.y >> 5)) & uint(0x3FFF);
				uint uv_y = (((info.y << 8) & uint(0x1F00)) | info.x) & uint(0x1FFF);
			  // uint tex_id = (info.x & uint(0xF8000000)) >> 27;
				// uint uv_x =   (info.x & uint(0x07FFE000)) >> 13;
				// uint uv_y =    info.x & uint(0x00001FFF);

				v_tex_id = tex_id;
				v_info = info;
				debug_uv = uvec2(uv_x, uv_y);
				v_uv = vec2(float(uv_x) / 16383.0, float(uv_y) / 8191.0);
			} else {
			  v_uv = data[gl_VertexID % 3];
			  v_col = col;
			}

			v_type = type;
			gl_Position = vec4(float(pos.x) / u_screen.x * 2 - 1.0, float(y) / u_screen.y * 2 + 1.0, 1.0, 1.0);
		}
		// CGLSLEND
	),

	.frag = GLSL(
		// CGLSLSTART
		precision mediump float;
		layout (location = 0) out vec4 color;

		in vec2 v_uv;
		in vec4 v_col;
		flat in uint v_type;
		flat in uint v_tex_id;
		// in vec2 v_pos;
		// in float v_idx;

		uniform sampler2D u_tex[32];
		// uniform vec2 u_tex_size[32];
		vec4 text;

		// https://github.com/memononen/nanovg/blob/f93799c078fa11ed61c078c65a53914c8782c00b/src/nanovg_gl.h#L555
		// float sdroundrect(vec2 pt, vec2 ext, float rad) {
		// 	vec2 ext2 = ext - vec2(rad,rad);
		// 	vec2 d = abs(pt) - ext2;
		// 	return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;
		// }

		void main() {
			if (v_type == uint(3)) {
			  text = texture(u_tex[v_tex_id], v_uv);
			  color = text;
				return;
			}
			color = vec4(1.0, 1.0, 1.0, 1.0);
		}
		// CGLSLEND
	)
};

struct gfx_layoutelement {
	GLenum type;
	u32 count;
	bool normalized;
	bool integer;
};

static inline u32 gfx_glsizeof(GLenum type) {
	switch(type) {
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
		case GL_RED:
			return 1;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			return 2;
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
		if (elems[i].integer) glVertexAttribIPointer(i, elems[i].count, elems[i].type, stride, (const void*) offset);
		else glVertexAttribPointer(i, elems[i].count, elems[i].type, elems[i].normalized ? GL_TRUE : GL_FALSE, stride, (const void*) offset);
		offset += elems[i].count * gfx_glsizeof(elems[i].type);
	}
}

static inline GLuint gfx_compshader(GLenum type, const char* src) {
	GLuint id = glCreateShader(type);
	glShaderSource(id, 1, &src, NULL);
	glCompileShader(id);

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

// ----------------------------------- Small Font Atlas Library ----------------------------------- //

// Algorithm's concept picked up from: https://blackpawn.com/texts/lightmaps/default.html
// Resizing texture picked up from here: https://straypixels.net/texture-packing-for-fonts/
// Could use this algorithm with "unused spaces" from here but would require a reimpl: https://github.com/TeamHypersomnia/rectpack2D/
// gfx_atlas_insert(vnew(), &(struct gfx_atlas_node) { {x, y}, {w, h}, {NULL, NULL}, false }, &(gfx_vector) {w, h})
static inline struct gfx_atlas_node* gfx_atlas_insert(gfx_atlas* atlas, u32 parent, gfx_vector_mini* size) {
  PROFILER_ZONE_START
	gfx_atlas_node* prnt = atlas->tree + parent;

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
		gfx_vector_mini realsize = prnt->s;
		if (prnt->p.x + prnt->s.w == USHRT_MAX) realsize.w = (GFX_ATLAS_START_SIZE * atlas->growth_factor) - prnt->p.x;
		if (prnt->p.y + prnt->s.h == USHRT_MAX) realsize.h = (GFX_ATLAS_START_SIZE * atlas->growth_factor) - prnt->p.y;

		// If the image doesn't fit in the node
		if(realsize.x < size->x || realsize.y < size->y) return NULL;

		// Defines children
		struct gfx_atlas_node c1 = { .p = prnt->p };
		struct gfx_atlas_node c2 = {};

		int dw = realsize.x - size->x;
		int dh = realsize.y - size->y;

		// To give the rest of the nodes more space, we need to split where there's more room
		bool vertsplit = dw > dh;

		// If the last texture is perfectly going to fit into the last available spot in the texture, split to maximize space (one of them is USHRT_MAXes)
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
		const u32 len = vlen(atlas->tree);
		vpusharr(atlas->tree, { c1, c2 }); // THIS MODIFIES THE POINTER SO YOU CANNOT USE PRNT BECAUSE IT USES THE OLD PTR
		atlas->tree[parent].child[0] = len;
		atlas->tree[parent].child[1] = len + 1;
		PROFILER_ZONE_END
		return gfx_atlas_insert(atlas, len, size);
	}

	struct gfx_atlas_node* ret = gfx_atlas_insert(atlas, prnt->child[0], size) ?: gfx_atlas_insert(atlas, prnt->child[1], size);
	PROFILER_ZONE_END
	return ret;
}


// ------------------------------------------ Util Funcs ------------------------------------------

// Aligns to the next multiple of a, where a is a power of 2
static inline u32 gfx_align(u32 n, u32 a) { return (n + a - 1) & ~(a - 1); }
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

static inline u32 gfx_totalarea(gfx_atlas_added* boxes /* Vector<gfx_atlas_added> */) {
	u32 total = 0;
	for(u32 i = 0; i < vlen(boxes); i ++)
		total += boxes[i].size.w * boxes[i].size.h;
	return total;
}

size_t gfx_read(char const* file, char** buf) {
	FILE *fp = fopen(file, "rb");
	if (!fp) {
		error("Couldn't open file %s", file);
		return 0;
	}

	// Finds length of file
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	rewind(fp);

	*buf = GFX_MALLOC(len * sizeof(char) + 1);
	*buf[len] = '\0';
	fread(*buf, 1, len, fp);
	fclose(fp);

	// returns both
	return len;
}

// Hashing function for all hash tables
static inline uint32_t hash_h_hash_func(const char * data, uint32_t len) { return XXH64(data, len, 764544365); }

void gfx_sleep(u32 milliseconds) {
  if(!milliseconds) return;
	#ifdef _WIN32
		ULONG res;
		NtSetTimerResolution(5010, TRUE, &res);
		Sleep(milliseconds);
		NtSetTimerResolution(5010, FALSE, &res);
	#elif _POSIX_C_SOURCE >= 199309L
		struct timespec ts;
		ts.tv_sec = milliseconds / 1000;
		ts.tv_nsec = (milliseconds % 1000) * 1000000;
		nanosleep(&ts, NULL);
	#else
		usleep(milliseconds * 1000);
	#endif
}

static bool gfx_get_app_key() {// Returning false here would mean an error occurred, so we check for it later on.
	if(!ctx->key) {
		char* key_path = alloca(strlen(ctx->settings.app_name) + sizeof("Software\\"));
		key_path[0] = 0;
		strcat(key_path, "Software\\");
		strcat(key_path, ctx->settings.app_name);
		if(RegCreateKeyExA(HKEY_CURRENT_USER, key_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &ctx->key, NULL)) return false;
	}
	return true;
}

bool gfx_store_set(gfx_store_type type, char* key_name, void* value, u32 size) {
	if(!gfx_get_app_key()) return false;
	if(RegSetValueExA(ctx->key, key_name, 0,
		 type == GFX_TYPE_INT ? REG_DWORD : type == GFX_TYPE_STRING ? REG_SZ : type == GFX_TYPE_BINARY ? REG_BINARY : REG_QWORD,
		 value, size)) return false;
	return true;
}

void* gfx_store_get(char* key_name, void* buf, u32* len) {
	if(!gfx_get_app_key()) return false;
	unsigned long winlength = len ? *len : 0;
	if(RegQueryValueExA(ctx->key, key_name, 0, NULL, buf, &winlength)) return NULL;
	if(len) *len = winlength;
	return buf;
}

void gfx_store_cleanup() {
	if(ctx->key) RegCloseKey(ctx->key);
}

// ---------------------------------- OpenGL Uniforms Functions ---------------------------------- //

// Gets the location from cache or sets it in cache
static inline GLint gfx_uloc(char* name) {
	GLint* v = hget(gfx_uni, ctx->gl.uniforms, name);
	if(!v) {
  	u32 id = glGetUniformLocation(ctx->gl.progid, name);
	  *hput(gfx_uni, ctx->gl.uniforms, name) = id;
		return id;
	}
	return *v;
}

static inline void gfx_usetm4(char* name, mat4x4 p) { glUniformMatrix4fv(gfx_uloc(name), 1, GL_FALSE, (const GLfloat*) p); }
static inline void gfx_useti(char* name, unsigned int p) { glUniform1i(gfx_uloc(name), p); }
static inline void gfx_usetiv(char* name, int* p, u32 count) { glUniform1iv(gfx_uloc(name), count, p); }
static inline void gfx_uset3f(char* name, vec3 p) { glUniform3f(gfx_uloc(name), p[0], p[1], p[2]); }
static inline void gfx_uset2f(char* name, vec2 p) { glUniform2f(gfx_uloc(name), p[0], p[1]); }

// --------------------------------------- Input Functions --------------------------------------- //

#if defined(_MSC_VER) && !defined(__clang__)
  void setup__(void) {}
# pragma comment(linker, "/alternatename:on_mouse_button=setup__ /alternatename:on_mouse_move=setup__ /alternatename:on_mouse_enter=setup__ /alternatename:on_mouse_scroll=setup__ /alternatename:on_key_button=setup__ /alternatename:on_key_char=setup__ /alternatename:on_window_resize=setup__ /alternatename:on_window_move=setup__")
#else
	void on_mouse_button(gfx_vector pos, gfx_mouse_button button, bool pressed, gfx_keymod mods) __attribute__((weak));
	void on_mouse_move(gfx_vector pos) __attribute__((weak));
	void on_mouse_enter(bool entered) __attribute__((weak));
	void on_mouse_scroll(gfx_vector scroll) __attribute__((weak));
	void on_key_button(gfx_keycode key, bool pressed, gfx_keymod mods) __attribute__((weak));
	void on_key_char(uint32_t utf_codepoint) __attribute__((weak));
	void on_window_resize(gfx_vector size) __attribute__((weak));
	void on_window_move(gfx_vector pos) __attribute__((weak));
#endif

gfx_vector gfx_mouse() {
	double xpos, ypos;
	glfwGetCursorPos(ctx->window, &xpos, &ypos);
	return (gfx_vector) { xpos, ypos };
}
char const *const gfx_key_name(gfx_keycode key) {
	return glfwGetKeyName(key, 0);
}

double gfx_time() { return glfwGetTime(); }
gfx_vector gfx_screen_dims() {
	return (gfx_vector) { .w = ctx->width, .h = ctx->height };
}

// Creates a projection in proportion to the screen coordinates
void gfx_updatescreencoords(u32 width, u32 height) {
	ctx->width = width; ctx->height = height;
	gfx_uset2f("u_screen", (vec2) { width, height });
}

void gfx_mousebuttoncallback(GLFWwindow* window, int button, int action, int mods) {
	if (on_mouse_button) on_mouse_button((gfx_vector) { ctx->mouse.x, ctx->mouse.y }, button, action == GLFW_PRESS, mods);
	if (action == GLFW_PRESS) {
		ctx->mouse.pressed = true;
		glfwGetCursorPos(window, &ctx->mouse.press.x, &ctx->mouse.press.y);
	} else ctx->mouse.pressed = false;
}

void gfx_cursorposcallback(GLFWwindow* window, double xpos, double ypos) {
	ctx->mouse.x = xpos;
	ctx->mouse.y = ypos;
	if(on_mouse_move) on_mouse_move((gfx_vector) { ctx->mouse.x, ctx->mouse.y });
	if(ctx->mouse.pressed) {
		int wxpos, wypos;
		glfwGetWindowPos(window, &wxpos, &wypos);
		glfwSetWindowPos(window, (int) (xpos - ctx->mouse.press.x) + wxpos, (int) (ypos - ctx->mouse.press.y) + wypos);
	}
}

void gfx_keycallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if(on_key_button && action != GLFW_REPEAT) on_key_button(key, action == GLFW_PRESS, mods);
	else if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, 1);
}

void gfx_framebuffersizecallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	gfx_updatescreencoords(width, height);
}

void gfx_windowposcallback(GLFWwindow* window, int xpos, int ypos) {
	if(on_window_move) on_window_move((gfx_vector) { .x = xpos, .y = ypos });

	if(!ctx->settings.dont_store_settings) {
		gfx_store_set(GFX_TYPE_INT, "pos_x", &(u32) { xpos }, sizeof(u32));
		gfx_store_set(GFX_TYPE_INT, "pos_y", &(u32) { ypos }, sizeof(u32));
	}
}

// --------------------------------------- Setup Functions --------------------------------------- //

char const *const default_fonts[] = {
#ifdef _WIN32
	"C:\\Windows\\Fonts\\segoeui.ttf"
#endif
};

FT_Library ft = NULL;


void gfx_ctx_set(struct gfx_ctx* c) {
	ctx = c;
	glBindVertexArray(c->gl.varrid);
	glUseProgram(c->gl.progid);
}

// Toggling Fullscreen:
// https://discourse.glfw.org/t/positioning-an-initially-fullscreen-window/1580/4

static gfx_settings default_settings = {
	.width = 800,
	.height = 600,
	.no_resize = false,
	.vsync = false,
	.no_decorations = false,
	.initial_window = GFX_WIN_DEFAULT,
	.dont_store_settings = false,
	.fps_recalc_delta = 0.1f,
};

struct gfx_ctx* gfx_init(const char* title, gfx_settings* settings) {
	GLFWwindow* window;
	DEBUG_MODE(glfwSetErrorCallback(glfwErrorHandler))
	CHECK_CALL(!glfwInit(), return NULL, "Couldn't initialize glfw3.");

	if(!title || !title[0]) title = "2DGFX No Title Provided";
	if(!settings) settings = &default_settings;
	if(!settings->app_name || !settings->app_name[0]) settings->app_name = title;

	struct gfx_ctx* old_ctx = ctx;
	ctx = GFX_CALLOC(1, sizeof(struct gfx_ctx));
	memcpy(&ctx->settings, settings, sizeof(gfx_settings));

	int width = settings->width, height = settings->height, pos_x = INT_MAX, pos_y = INT_MAX;
	if(!settings->dont_store_settings) {
		u32 len = sizeof(u32);
		if(!settings->no_resize) {
			gfx_store_get("width", &width, &len);
			gfx_store_get("height", &height, &len);
		}
		gfx_store_get("pos_x", &pos_x, &len);
		gfx_store_get("pos_y", &pos_y, &len);
		info("Setting Window parameters to x: %d, y: %d (%dx%d)", pos_x, pos_y, width, height);
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	DEBUG_MODE(glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE))

	if(settings->transparent) glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
	if(settings->no_decorations) glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	if(settings->no_resize) glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	if(pos_x != INT_MAX) {
		glfwWindowHint(GLFW_POSITION_X, pos_x);
		glfwWindowHint(GLFW_POSITION_Y, pos_y);
	}



	// Create a windowed mode window and its OpenGL context
	CHECK_CALL(!(window = glfwCreateWindow(width, height, title, NULL, NULL)),
		glfwTerminate(); free(ctx); ctx = old_ctx; return NULL, "Couldn't initialize GLFW window.");
	glfwMakeContextCurrent(window);

	// GLFW input callbacks
	glfwSetMouseButtonCallback(window, gfx_mousebuttoncallback);
	glfwSetCursorPosCallback(window, gfx_cursorposcallback);
	glfwSetFramebufferSizeCallback(window, gfx_framebuffersizecallback);
	glfwSetWindowPosCallback(window, gfx_windowposcallback);
	glfwSetKeyCallback(window, gfx_keycallback);


	// Set the framerate to the framerate of the screen, basically 60fps.
	// glfwSwapInterval(1);

	GLenum err;
	CHECK_CALL((err = glewInit()), glfwTerminate(); free(ctx); ctx = old_ctx; return NULL, "GLEW initialization failed: %s", glewGetErrorString(err));

	PROFILER_GPU_INIT()

	// Debug callback for detecting OpenGL errors
	DEBUG_MODE(glDebugMessageCallback(glDebugMessageHandler, NULL))

	// Enables OpenGL Features
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE); // CULL FACE causes everything to be distorted, only half the rects show up for some reason
	// glEnable(GL_DEPTH_TEST);

	// Fills application context struct
	ctx->gl.progid = gfx_shaderprog(default_shaders.vert, default_shaders.frag);
	glGenVertexArrays(1, &ctx->gl.varrid);
	ctx->textures = vnew();
	ctx->atlases = vnew();
	ctx->font.store = vnew();
	ctx->gl.drawbuf.shp = vnew();
	ctx->gl.drawbuf.idx = vnew();
	ctx->font.size = 48;
	ctx->window = window;

	// for(int i = 0; i < sizeof(ctx->gl.slots) / sizeof(ctx->gl.slots[0]); i ++)
	// 	ctx->gl.slots[i] = -1;

	// Initializes OpenGL Context
	gfx_ctx_set(ctx);

	// Text atlas
	gfx_updatescreencoords(width, height);
	fill(255, 255, 255, 255);
	return ctx;
}

void gfx_setting_set(enum gfx_setting_name setting, void *const value) {
	switch(setting) {
		case GFX_SETTING_VSYNC: glfwSwapInterval(ctx->settings.vsync = *(bool*)value); break;
		case GFX_SETTING_NO_RESIZE: glfwSetWindowAttrib(ctx->window, GLFW_RESIZABLE, ctx->settings.no_resize = *(bool*)value); break;
		case GFX_SETTING_NO_DECORATIONS: glfwSetWindowAttrib(ctx->window, GLFW_DECORATED, ctx->settings.no_decorations = *(bool*)value); break;
		case GFX_SETTING_TRANSPARENT: glfwSetWindowAttrib(ctx->window, GLFW_TRANSPARENT_FRAMEBUFFER, ctx->settings.transparent = *(bool*)value); break;
		case GFX_SETTING_DONT_STORE_SETTINGS: ctx->settings.dont_store_settings = *(bool*)value; break;
		case GFX_SETTING_INITIAL_WINDOW: ctx->settings.initial_window = *(enum gfx_setting_initial_window_mode*)value; break;
		case GFX_SETTING_APP_NAME: ctx->settings.app_name = (const char*)value; break; // Need to put in docs that only strings that live on for the duration of the program are allowed.
		default: break;
	}


}


static void draw();
bool gfx_frame() {
	PROFILER_ZONE_START
	if(ctx->frame.count > 0) {
		draw();
		PROFILER_GPU_ZONE_START("swapbuffers")
		glfwSwapBuffers(ctx->window);
		PROFILER_GPU_ZONE_END()
		if(ctx->frame.count % 50 == 0)
			PROFILER_GPU_QUERIES_COLLECT()
		glfwPollEvents();
	}
	PROFILER_FRAME_MARK
	PROFILER_GPU_ZONE_START("GLClear")
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	PROFILER_GPU_ZONE_END()
	ctx->frame.count ++;
	ctx->frame.start = glfwGetTime();
	ctx->frame.delta = ctx->frame.start - ctx->frame.last;
	ctx->frame.last  = ctx->frame.start;

	PROFILER_ZONE_END
	if (glfwWindowShouldClose(ctx->window)) {
		PROFILER_GPU_QUERIES_COLLECT()
		return false;
	}
	return true;
}

void gfx_quit() {
	glfwSetWindowShouldClose(ctx->window, 1);
}

bool gfx_fps_changed() {
	return ctx->frame.start - ctx->frame.lastfpscalc >= ctx->settings.fps_recalc_delta;
}

double gfx_fps() {
	double rn = ctx->frame.start;
	if(rn - ctx->frame.lastfpscalc < ctx->settings.fps_recalc_delta || ctx->frame.count - ctx->frame.lastfpscount == 0)
		return ctx->frame.lastfpsnum;
	ctx->frame.lastfpsnum = ((double) ctx->frame.count - ctx->frame.lastfpscount) / (rn - ctx->frame.lastfpscalc);
	ctx->frame.lastfpscount = ctx->frame.count;
	ctx->frame.lastfpscalc = rn;
	return ctx->frame.lastfpsnum;
}

void gfx_default_fps_counter() {
	static char fps[24] = {0};
	const u32 oldfontsize = ctx->font.size;
	if(gfx_fps_changed())
		snprintf(fps, 23, "%.2f fps", gfx_fps());
	font_size(20);
	text(fps, ctx->width - 150, ctx->height - 10);
	font_size(oldfontsize);
}
















// -------------------------------------- Graphics Functions -------------------------------------- //

static inline void gfx_draw_setup() {
	glGenBuffers(1, &ctx->gl.vbufid);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->gl.vbufid);
	glGenBuffers(1, &ctx->gl.idxbufid);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->gl.idxbufid);

	// REMEMBER TO CHANGE THE NUMBER AT THE FRONT ITS INSANELY IMPORTANT
	// struct gfx_layoutelement {
	// 	GLenum type;
	// 	u32 count;
	// 	bool normalized;
	// 	bool integer;
	// };
	gfx_applylayout(2, (struct gfx_layoutelement[]) {
		{ GL_SHORT,          2, false, true }, // X, Y
		// { GL_UNSIGNED_SHORT, 1, true,  false }, // Multipurpose (Stroke) (future maybe)
		// { GL_UNSIGNED_BYTE,  1, false, true  }, // Type of texture.
		// { GL_UNSIGNED_BYTE,  1, false, true  }, // Texture Index in the array of texture samplers
		// { GL_UNSIGNED_SHORT, 2, true,  false }, // Texture X, Texture Y
		{ GL_UNSIGNED_BYTE,  4, false, true }  // Color (RGBA)
	});
	gfx_usetiv("u_tex", (int[]) { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 }, 32);
}

static void gfx_update_atlas(gfx_atlas* atlas);
static inline void gfx_make_tex_active(gfx_tex_id tex_id);
static inline void draw() {
	u32 slen = vlen(ctx->gl.drawbuf.shp);
	u32 ilen = vlen(ctx->gl.drawbuf.idx);
	if(!slen) return;
	PROFILER_ZONE_START
	PROFILER_GPU_ZONE_START("draw")

	// Upload all texture atlas updates
	for(u32 i = 0; i < vlen(ctx->atlases); i ++) {
		gfx_atlas* atlas = ctx->atlases + i;
		if(vlen(atlas->added)) {
			gfx_make_tex_active(atlas->tex_id);
			gfx_update_atlas(atlas);
		}
	}

	if(!ctx->gl.vbufid) gfx_draw_setup();

	// Vertex buffer upload
	if(ctx->gl.drawbuf.maxdrawbufsize.w < slen) {
		glBufferData(GL_ARRAY_BUFFER, slen * sizeof(*ctx->gl.drawbuf.shp), ctx->gl.drawbuf.shp, GL_DYNAMIC_DRAW);
		ctx->gl.drawbuf.maxdrawbufsize.w = slen;
	} else glBufferSubData(GL_ARRAY_BUFFER, 0, slen * sizeof(*ctx->gl.drawbuf.shp), ctx->gl.drawbuf.shp);

	// Element array buffer upload
	if(ctx->gl.drawbuf.maxdrawbufsize.h < ilen) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ilen * sizeof(*ctx->gl.drawbuf.idx), ctx->gl.drawbuf.idx, GL_DYNAMIC_DRAW);
		ctx->gl.drawbuf.maxdrawbufsize.h = ilen;
	} else glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, ilen * sizeof(*ctx->gl.drawbuf.idx), ctx->gl.drawbuf.idx);

	// Draw call
	glDrawElements(GL_TRIANGLES, ilen, GL_UNSIGNED_INT, NULL);

	// Reset draw buffers and Z axis
	vempty(ctx->gl.drawbuf.shp);
	vempty(ctx->gl.drawbuf.idx);
	// ctx->gl.drawbuf.idxstart = vlen(ctx->gl.drawbuf.idx);
	PROFILER_GPU_ZONE_END()
	PROFILER_ZONE_END
}



void fill(u8 r, u8 g, u8 b, u8 a) {
	ctx->curcol.r = r;
	ctx->curcol.g = g;
	ctx->curcol.b = b;
	ctx->curcol.a = a;
}

void quad(short x1, short y1, short x2, short y2, short x3, short y3, short x4, short y4) {
	PROFILER_ZONE_START
	u32 cur_idx = vlen(ctx->gl.drawbuf.shp);
	vpusharr(ctx->gl.drawbuf.idx, { cur_idx, cur_idx + 1, cur_idx + 2, cur_idx + 2, cur_idx, cur_idx + 3 });
	vpusharr(ctx->gl.drawbuf.shp, {
		{ .x = x1, .y = y1, .type = GFX_FULL, .col = { .full = ctx->curcol.full } },
		{ .x = x2, .y = y2, .type = GFX_FULL, .col = { .full = ctx->curcol.full } },
		{ .x = x3, .y = y3, .type = GFX_FULL, .col = { .full = ctx->curcol.full } },
		{ .x = x4, .y = y4, .type = GFX_FULL, .col = { .full = ctx->curcol.full } }
	});
	PROFILER_ZONE_END
}

void rect(short x, short y, short w, short h) {
	quad(x, y, x + w, y, x + w, y + h, x, y + h);
}

// Rect with stroke
void rect_s(short x, short y, short w, short h, short s) {
	int x_left = x + s,
			x_right = x + w - s,
			y_top = y + s,
			y_bottom = y + h - s;
	quad(x, y, x + w, y, x_right, y_top, x_left, y_top); // top
	quad(x, y, x_left, y_top, x_left, y_bottom, x, y + h); // left
	quad(x + w, y, x_right, y_top, x_right, y_bottom, x + w, y + h); // right
	quad(x, y + h, x_left, y_bottom, x_right, y_bottom, x + w, y + h); // bottom
}

void background(u8 r, u8 g, u8 b, u8 a) {
	u32 tmp = ctx->curcol.full;
	fill(r, g, b, a);
	rect(0, 0, ctx->width, ctx->height);
	ctx->curcol.full = tmp;
}

// Simplify sin^4 + cos^4
// https://math.stackexchange.com/a/1458312
#define PI 3.14159265358979f
// void ellipse(short x, short y, short rx, short ry) {
// 	PROFILER_ZONE_START
// 	u32 cur_idx = vlen(ctx->gl.drawbuf.shp);

// 	short biggest_r = max(rx, ry);

// 	// https://github.com/grimfang4/sdl-gpu/blob/master/src/renderer_shapes_GL_common.inl#L79
// 	float dt = 0.625f / sqrtf(biggest_r);
// 	u32 numsegments = 2.0f * PI / dt + 1.0f;
// 	if(numsegments < 16)
// 		numsegments = 16, dt = 2.0f*PI/15.0f;


// 	float cosdt = cosf(dt);
// 	float sindt = sinf(dt);
// 	float dx = cosdt / 2, dy = sindt * sindt / 2, tmpdx;
// 	u32 i = 2;

// 	// First triangle, beginning of second triangle
// 	vpusharr(ctx->gl.drawbuf.shp, {
// 		{ .pos = { x, y }, .type = SHAPE, // Center
// 			.col = { .full = ctx->curcol.full } },
// 		{ .pos = { x + rx / 2, y }, .type = SHAPE, // First edge point
// 			.col = { .full = ctx->curcol.full } }
// 	});

// 	while (i <= numsegments) {
// 		vpush(ctx->gl.drawbuf.shp, {
// 			.pos = { x + rx * dx, y + ry * dy }, .type = SHAPE,
// 			.col = { .full = ctx->curcol.full }
// 		});
// 		vpusharr(ctx->gl.drawbuf.idx, { cur_idx, cur_idx + i - 1, cur_idx + i });

// 		tmpdx = cosdt * dx - sindt * dy;
// 		dy = sindt * dx + cosdt * dy;
// 		dx = tmpdx;
// 		i++;
// 	}

// 	// Last vertex
// 	vpush(ctx->gl.drawbuf.shp, {
// 		.pos = { x + rx * dx, y + ry * dy }, .type = SHAPE,
// 		.col = { .full = ctx->curcol.full }
// 	});

// 	// Last triangle, middle, last point, first point
// 	vpusharr(ctx->gl.drawbuf.idx, { cur_idx, cur_idx + i, cur_idx + 1 });
// 	PROFILER_ZONE_END
// }

// // https://stackoverflow.com/questions/51010740/how-to-optimize-circle-draw
// static inline u32 gfx_arc_partial(short x, short y, short rx, short ry, short degree, short stroke, bool last_vert) {
// 	short biggest_r = max(rx, ry);
// 	u32 cur_idx = vlen(ctx->gl.drawbuf.shp);

// 	float dt = 0.625f / sqrtf(biggest_r);
// 	u32 numsegments = degree / dt + 1.0f;
// 	if(numsegments < 16) numsegments = 16, dt = degree / 15.0f;

// 	float cosdt = cosf(dt);
// 	float sindt = sinf(dt);
// 	float dx = cosdt / 2, dy = sindt * cosdt / 2, ndx, ndy;
// 	float xf = x, yf = y, rxf = rx, ryf = ry, strokef = stroke;

// 	vpusharr(ctx->gl.drawbuf.shp, {
// 		{ .pos = { x + rx / 2, y }, .type = SHAPE, .col = { .full = ctx->curcol.full } },
// 		{ .pos = { x + rx / 2 - stroke, y + ry / 2 - stroke }, .type = SHAPE, .col = { .full = ctx->curcol.full } }
// 	});

// 	for(u32 i = 1; i < numsegments + last_vert; i ++) {
// 		ndx = cosdt * dx - sindt * dy;
// 		ndy = sindt * dx + cosdt * dy;

// 		vpusharr(ctx->gl.drawbuf.idx, { cur_idx + i * 2 - 2, cur_idx + i * 2, cur_idx + i * 2 - 1, cur_idx + i * 2, cur_idx + i * 2 - 1, cur_idx + i * 2 + 1 });
// 		vpusharr(ctx->gl.drawbuf.shp, {
// 			{ .pos = { xf + rxf * ndx, yf + ryf * ndy }, .type = SHAPE, .col = { .full = ctx->curcol.full } },
// 			{ .pos = { xf + rxf * ndx - strokef * ndx * 2, yf + ryf * ndy - strokef * ndy * 2 }, .type = SHAPE, .col = { .full = ctx->curcol.full } }
// 		});

// 		dx = ndx;
// 		dy = ndy;
// 	}
// 	return numsegments;
// }

// void ellipse_s(short x, short y, short rx, short ry, short stroke) {
// 	const u32 cur_idx = vlen(ctx->gl.drawbuf.shp);
// 	const u32 numsegments = gfx_arc_partial(x, y, rx, ry, 2 * PI, stroke, false);

// 	// Connects the final vertices of the arc to the
// 	vpusharr(ctx->gl.drawbuf.idx, { cur_idx, cur_idx + numsegments * 2 - 2, cur_idx + 1, cur_idx + numsegments * 2 - 2, cur_idx + 1, cur_idx + numsegments * 2 - 1 });
// }

// void arc(short x, short y, short rx, short ry, short degree, short stroke) {
// 	const u32 cur_idx = vlen(ctx->gl.drawbuf.shp);
// 	const u32 numsegments = gfx_arc_partial(x, y, rx, ry, degree, stroke, false);

// 	// Adds a vertex for the end, and connects to it.


// }



// void rrect(short x, short y, short w, short h, short r) {
// 	u32 cur_idx = vlen(ctx->gl.drawbuf.shp);

// }

// void circle(short x, short y, short r) {
// 	ellipse(x, y, r, r);
// }

// https://github.com/memononen/nanovg/blob/master/src/nanovg.c#L2077










// ------------------------------ Texture + Atlas Creation Functions ------------------------------ //

/*
 * System Architecture:
 * - Slot : One of the 32 texture slots available for drawing texels from that Opengl provides.
 * - Texture ID : The ID of the texture in OpenGL (1024 available, pretty much unreachable).
 * - 
 */

static inline void gfx_bind_slot(gfx_slot_hnd slot) { if(slot) glActiveTexture(GL_TEXTURE0 + (ctx->gl.slot_bound = slot) - 1); }
static inline void gfx_bind_tex(gfx_tex_id tex) {
	if(ctx->textures[tex].slot == ctx->gl.slot_bound) return;

	if(ctx->gl.slots[ctx->gl.slot_bound - 1])
		ctx->textures[ctx->gl.slots[ctx->gl.slot_bound - 1] - 1].slot = 0;

	ctx->gl.slots[ctx->gl.slot_bound - 1] = tex + 1;
	ctx->textures[tex].slot = ctx->gl.slot_bound;
	glBindTexture(GL_TEXTURE_2D, ctx->textures[tex].id);
	info("Bound texture #%d to slot #%d", tex, ctx->textures[tex].slot);
}
static inline gfx_slot_hnd gfx_find_empty_slot() {
	for(int i = 0; i < sizeof(ctx->gl.slots) / sizeof(ctx->gl.slots[0]); i ++)
		if(!ctx->gl.slots[i]) return i + 1;
	return 0;
}
static inline void gfx_make_tex_active(gfx_tex_id tex_id) {
	gfx_slot_hnd slot = ctx->textures[tex_id].slot;
	gfx_assert(slot, "Slot bound to texture #%d is invalid.", tex_id);
	gfx_bind_slot(slot);
}

static void gfx_tex_upload(u8* buf, u32 w, u32 h, GLenum format, bool pixellated) {
	glPixelStorei(GL_UNPACK_ALIGNMENT, gfx_glsizeof(format));
	glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, buf);

	// We just don't need mipmaps for fonts so we do this for normal images
	if(!pixellated) glGenerateMipmap(GL_TEXTURE_2D); // CALL AFTER UPLOAD
	info("Uploaded texture (%dx%d) to slot #%d", w, h, ctx->gl.slot_bound);
}

static gfx_tex_id gfx_tex_push(bool pixellated) {
	gfx_bind_slot(gfx_find_empty_slot());

	vpush(ctx->textures, {});
	gfx_tex_id tex_id = vlen(ctx->textures) - 1;
	glGenTextures(1, &ctx->textures[tex_id].id);
	gfx_bind_tex(tex_id);

	// Sets default params
	GLenum minmagfilter = pixellated ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minmagfilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, minmagfilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if(!pixellated) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	return tex_id;
}

static gfx_slot_hnd gfx_make_tex_available_for_draw(gfx_tex_id tex_id) {
  gfx_texture* tex = &ctx->textures[tex_id];
  if(!tex->slot) {
		tex->slot = gfx_find_empty_slot();
		if(!tex->slot) { draw(); tex->slot = 1; /* find_empty_slot will always return 1 here. */ }
		gfx_bind_slot(tex->slot);
		gfx_bind_tex(tex_id);
	}
	return tex->slot;
}

static u32 gfx_atlas_try_insert(gfx_atlas* tex_atlas, gfx_vector_mini* size, struct gfx_atlas_node** ret_maybe) {
	u32 growth = 1;
	do {
		*ret_maybe = gfx_atlas_insert(tex_atlas, 0, size);
		if (*ret_maybe) return growth;
		else if (tex_atlas->growth_factor >= GFX_ATLAS_MAX_GROWTH_FACTOR) return 0;

		growth *= 2;
		tex_atlas->growth_factor *= 2;
	} while(true);
}

static gfx_atlas* gfx_atlases_add(GLenum format, bool pixellated, gfx_vector_mini* size, gfx_vector_mini* ret_pos) {
	PROFILER_ZONE_START
	u32 growth = 0;
	gfx_atlas* atlas = NULL;
	struct gfx_atlas_node* maybe = NULL;

	for(int i = 0; i < vlen(ctx->atlases) && !growth; i ++)
		if(ctx->atlases[i].format == format)
			growth = gfx_atlas_try_insert((atlas = ctx->atlases + i), size, &maybe);

	if(!growth) {
		gfx_atlas_node* tree = vnew();
		vpush(tree, { .s = { USHRT_MAX, USHRT_MAX } });
		vpush(ctx->atlases, {
			.format = format,
			.tree = tree,
			.tex_id = gfx_tex_push(pixellated),
			.added = vnew(),
			.growth_factor = 1
		});

		atlas = vlast(ctx->atlases);
		gfx_assert(gfx_atlas_try_insert(atlas, size, &maybe), "Somehow couldn't find space to insert into the atlas, object probably too big (%dx%d).", size->x, size->y);

		atlas->buf = GFX_MALLOC(GFX_ATLAS_START_SIZE * GFX_ATLAS_START_SIZE * gfx_glsizeof(format) * sizeof(u8) * atlas->growth_factor);
		info("Generating new atlas #%d", vlen(ctx->atlases));

	} else if(growth > 1) {
		u8* new_buf = GFX_MALLOC(GFX_ATLAS_START_SIZE * GFX_ATLAS_START_SIZE * gfx_glsizeof(format) * sizeof(u8) * atlas->growth_factor);
		int old_w = GFX_ATLAS_START_SIZE * atlas->growth_factor / growth, old_h = GFX_ATLAS_START_SIZE * atlas->growth_factor / growth;
		int new_w = GFX_ATLAS_START_SIZE * atlas->growth_factor;

		for(int i = 0; i < old_h; i ++)
			memcpy(new_buf + i * new_w, atlas->buf + i * old_w, old_w);

		free(atlas->buf);
		atlas->buf = new_buf;
	}

	vpush(atlas->added, { maybe->p, maybe->s });

	*ret_pos = maybe->p;
	PROFILER_ZONE_END
	return atlas;
}

// Assumes texture is bound
static void gfx_update_atlas(gfx_atlas* atlas) {
	static _Thread_local char* buf = NULL;
	static _Thread_local int buf_size = 0;
	PROFILER_ZONE_START
	PROFILER_GPU_ZONE_START("update_atlas")

	u32 len = vlen(atlas->added);
	if(!atlas->uploaded || len > 10 && gfx_totalarea(atlas->added) > GFX_ATLAS_W(atlas) * atlas->growth_factor * atlas->growth_factor * GFX_ATLAS_START_SIZE / 3) {
		gfx_tex_upload(atlas->buf, GFX_ATLAS_W(atlas), GFX_ATLAS_START_SIZE * atlas->growth_factor, atlas->format, false);
		atlas->uploaded = true;
		vempty(atlas->added);
		PROFILER_ZONE_END
		return;
	}

	if(!buf) buf = GFX_MALLOC(GFX_ATLAS_START_SIZE * GFX_ATLAS_START_SIZE * gfx_glsizeof(atlas->format) * sizeof(u8) * atlas->growth_factor);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	for(u32 i = 0; i < len; i ++) {
		gfx_atlas_added* added = atlas->added + i;

		if(buf_size < added->size.w * added->size.h * gfx_glsizeof(atlas->format)) {
			buf_size = added->size.w * added->size.h * gfx_glsizeof(atlas->format);
			buf = GFX_REALLOC(buf, buf_size);
		}

		for(int j = 0; j < added->size.h; j ++)
			memcpy(buf + j * added->size.w * gfx_glsizeof(atlas->format), atlas->buf + (added->place.y + j) * GFX_ATLAS_W(atlas) + added->place.x * gfx_glsizeof(atlas->format),
						 added->size.w * gfx_glsizeof(atlas->format));

		glTexSubImage2D(GL_TEXTURE_2D, 0, added->place.x, added->place.y, added->size.w, added->size.h, atlas->format, GL_UNSIGNED_BYTE, buf);
		// info("Uploaded to atlas #%d at (%d, %d) (%dx%d)", atlas - ctx->atlases, added->place.x, added->place.y, added->size.w, added->size.h);
	}

	// stbi_write_png("bitmap.png", atlas->growth_factor * GFX_ATLAS_START_SIZE, atlas->growth_factor * GFX_ATLAS_START_SIZE, 1, atlas->buf, GFX_ATLAS_W(atlas));

	vempty(atlas->added);
	PROFILER_GPU_ZONE_END()
	PROFILER_ZONE_END
}























// --------------------------- OpenGL Texture Drawing Related Functions --------------------------- //

gfx_img gfx_load_img_rgba(u8* t, int width, int height) {
	gfx_internal_image img = { .size = { width, height } };

	// Load the image into an atlas if it's small enough
	if(width >= GFX_ATLAS_MAX_SIZE / 2 || height >= GFX_ATLAS_MAX_SIZE / 2) {
		gfx_atlas* atlas = gfx_atlases_add(GL_RGBA, false, &img.size, &img.place);

		for(int i = 0; i < height; i ++)
			memcpy(atlas->buf + img.place.x * gfx_glsizeof(atlas->format) + img.place.y * GFX_ATLAS_W(atlas), t, width * gfx_glsizeof(atlas->format));

		img.atlas_hnd = atlas - ctx->atlases + 1;
	}

	// Otherwise just upload it straight to the GPU
	else {
		img.tex_hnd = gfx_tex_push(false) + 1;
		gfx_tex_upload(t, width, height, GL_RGBA, false);
	}

	info("Loaded image (%dx%d) into %s #%d", width, height, img.atlas_hnd ? "atlas" : "texture", img.atlas_hnd ? img.atlas_hnd : img.tex_hnd);

	gfx_assert(img.tex_hnd || img.atlas_hnd, "Critical error loading image (%dx%d) for some reason.", width, height);
	stbi_image_free(t);

	if(!ctx->images) ctx->images = vnew();
	vpush(ctx->images, img);
	return vlen(ctx->images) - 1;
}


gfx_img gfx_load_img_mem(u8* t, u32 len) {
	int width, height, channels;
	u8* img = stbi_load_from_memory(t, len, &width, &height, &channels, STBI_rgb_alpha);
	if(!img) { error("Couldn't load image from memory."); return -1; }
	return gfx_load_img_rgba(img, width, height);
}

gfx_img gfx_load_img(const char* file) {
	int width, height, channels;
	// stbi_set_flip_vertically_on_load(true);
	u8* img = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
	if(!img) { error("Couldn't load image from memory."); return -1; }
	info("Loaded image '%s' (%dx%d)", file, width, height);
	return gfx_load_img_rgba(img, width, height);
}



void image(gfx_img img_id, short x, short y, short w, short h) {
	if(img_id < 0) return;
	PROFILER_ZONE_START

	gfx_internal_image* img = ctx->images + img_id;
	GLuint tex_id = img->tex_hnd ? img->tex_hnd - 1 : ctx->atlases[img->atlas_hnd - 1].tex_id;
	// gfx_vector_mini tcoords[4] = {
	// 	{ .w = 0,        .h = 0,        },
	// 	{ .w = UV_X_MAX, .h = 0,        },
	// 	{ .w = UV_X_MAX, .h = UV_Y_MAX, },
	// 	{ .w = 0,        .h = UV_Y_MAX, },
	// };

	gfx_slot_hnd slot = gfx_make_tex_available_for_draw(tex_id);

	// This code is for when Images automatically get allocated to atlases. It should work right now but complicates things so it's disabled.
	// if(img->atlas_hnd) {
	// 	gfx_atlas* atlas = ctx->atlases + img->atlas_hnd - 1;
	// 	tcoords[0] = (gfx_vector_mini) { .w = img->place.x,               .h = img->place.y };
	// 	tcoords[1] = (gfx_vector_mini) { .w = img->place.x + img->size.w, .h = img->place.y };
	// 	tcoords[2] = (gfx_vector_mini) { .w = img->place.x + img->size.w, .h = img->place.y + img->size.h };
	// 	tcoords[3] = (gfx_vector_mini) { .w = img->place.x,               .h = img->place.y + img->size.h };
	// }

	// Creates a rectangle the image will be held on, then uploads the texture coordinates to map to the image
	u32 cur_idx = vlen(ctx->gl.drawbuf.shp);
	vpusharr(ctx->gl.drawbuf.idx, { cur_idx, cur_idx + 1, cur_idx + 2, cur_idx + 2, cur_idx, cur_idx + 3 });
	vpusharr(ctx->gl.drawbuf.shp, {
		{ .x = x    , .y = y    , .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = 0,        .uv_y = 0,        },
		{ .x = x + w, .y = y    , .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = UV_X_MAX, .uv_y = 0,        },
		{ .x = x + w, .y = y + h, .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = UV_X_MAX, .uv_y = UV_Y_MAX, },
		{ .x = x    , .y = y + h, .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = 0,        .uv_y = UV_Y_MAX, },
	});
	PROFILER_ZONE_END
}

gfx_vector_mini* const isize(gfx_img img) { return &ctx->images[img].size; }










// ------------------------------------ Text Drawing Functions ------------------------------------ //

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

static gfx_char* gfx_load_char(gfx_face tf, FT_ULong c) {
	PROFILER_ZONE_START
	gfx_typeface* face = ctx->font.store + tf;
	CHECK_CALL(FT_Set_Pixel_Sizes(face->face, 0, ctx->font.size * 4.0f / 3.0f), PROFILER_ZONE_END; return NULL, "Couldn't set size");
	CHECK_CALL(FT_Load_Char(face->face, c, FT_LOAD_RENDER), PROFILER_ZONE_END; return NULL, "Couldn't load char '%c' (%X)", c, c);

	int width = face->face->glyph->bitmap.width;
	int height = face->face->glyph->bitmap.rows;

	// Tries to add the character, resizing the whole texture until it's done
	gfx_vector_mini pos;
	gfx_vector_mini size = { width, height };
	gfx_atlas* atlas = gfx_atlases_add(GL_RED, true, &size, &pos);
	// info("Loaded character '%c' (%X) at (%d, %d) in atlas #%d", c, c, pos.x, pos.y, atlas - ctx->atlases);

	// Writes the character's pixels to the atlas buffer.
	u8* insertsurface = atlas->buf + pos.x + pos.y * (GFX_ATLAS_START_SIZE * atlas->growth_factor);
	for(int i = 0; i < height; i ++)
		memcpy(insertsurface + i * (GFX_ATLAS_START_SIZE * atlas->growth_factor), face->face->glyph->bitmap.buffer + i * width, width);

	gfx_char* inserted;
	*(inserted = hput(gfx_char, face->chars, { c, ctx->font.size })) = (gfx_char) {
		.c = c, .size = size,
		.bearing = {
			.x = face->face->glyph->bitmap_left,
			.y = face->face->glyph->bitmap_top
		},
		.advance = (face->face->glyph->advance.x >> 6),
		.place = pos,
		.atlas = atlas - ctx->atlases,
	};
	PROFILER_ZONE_END
	return inserted;
}

gfx_face gfx_load_font(const char* file) {
	PROFILER_ZONE_START
	gfx_typeface new = {
		.name = file,
		.chars = {0},
		.face = NULL
	};

	// Loads the freetype library.
	if(!ft) CHECK_CALL(FT_Init_FreeType(&ft), return -1, "Couldn't initialize freetype");

	// Loads the new face in using the library
	CHECK_CALL(FT_New_Face(ft, file, 0, (FT_Face*) &new.face), return -1, "Couldn't load font '%s'", file);
	CHECK_CALL(FT_Set_Pixel_Sizes(new.face, 0, RENDERING_FONT_SIZE()), return -1, "Couldn't set size");

	// Adds space_width
	CHECK_CALL(FT_Load_Char(new.face, ' ', FT_LOAD_RENDER), return -1, "Couldn't load the Space Character ( )");
	new.space_width = new.face->glyph->advance.x >> 6;

	// Stores the font
	vpush(ctx->font.store, new);
	info("Loaded font '%s'", new.name);
	PROFILER_ZONE_END
	return vlen(ctx->font.store) - 1;
}


void font_size(u32 size) { if(size) ctx->font.size = size; }
void line_height(f32 h) { ctx->font.lh = h; }
void font(gfx_face face, u32 size) {
	if(face >= 0 && face != ctx->font.cur)
		ctx->font.cur = face;
	if(size > 0) ctx->font.size = size;
}
void text(const char* str, short x, short y) {
	if(!vlen(ctx->font.store)) return;
	PROFILER_ZONE_START

	gfx_typeface* face = ctx->font.store + ctx->font.cur;

	FT_ULong point;
	short curx = x, cury = y;
	short realx, realy, w, h;
	u16 tx, ty, tw, th;
	while ((point = gfx_readutf8((u8**) &str))) {

		// Newlines in five lines :D
		if (point == '\n') {
			cury += ctx->font.lh * ctx->font.size * 4 / 3;
			curx = x;
			continue;
		}

		else if(point == ' ') {
			curx += face->space_width * ctx->font.size * 4 / 3 /*px -> pts*/ / RENDERING_FONT_SIZE();
			continue;
		}

		gfx_char* ch = hget(gfx_char, face->chars, { point, ctx->font.size });
		if(!ch) ch = gfx_load_char(ctx->font.cur, point);
		if(!ch) continue;

		gfx_atlas* atlas = ctx->atlases + ch->atlas;
		gfx_slot_hnd slot = gfx_make_tex_available_for_draw(atlas->tex_id);

		realx = curx + ch->bearing.x;
		realy = cury - ch->bearing.y;
		w     = ch->size.x;
		h     = ch->size.y;


		tx = (float) ch->place.x * (float) (UV_X_MAX / (float) (atlas->growth_factor * GFX_ATLAS_START_SIZE));
		ty = (float) ch->place.y * (float) (UV_Y_MAX / (float) (atlas->growth_factor * GFX_ATLAS_START_SIZE));
		tw = (float) ch->size.x  * (float) (UV_X_MAX / (float) (atlas->growth_factor * GFX_ATLAS_START_SIZE));
		th = (float) ch->size.y  * (float) (UV_Y_MAX / (float) (atlas->growth_factor * GFX_ATLAS_START_SIZE));

		u32 cur_idx = vlen(ctx->gl.drawbuf.shp);
		vpusharr(ctx->gl.drawbuf.idx, { cur_idx, cur_idx + 1, cur_idx + 2, cur_idx + 2, cur_idx, cur_idx + 3 });
		vpusharr(ctx->gl.drawbuf.shp, {
			{ .x = realx    , .y = realy    , .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = tx,      .uv_y = ty },
			{ .x = realx + w, .y = realy    , .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = tx + tw, .uv_y = ty },
			{ .x = realx + w, .y = realy + h, .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = tx + tw, .uv_y = ty + th },
			{ .x = realx    , .y = realy + h, .type = GFX_TEX, .tex_slot = slot - 1, .uv_x = tx,      .uv_y = ty + th }
		});

		// Advance cursors for next glyph
		curx += ch->advance;
	}
	PROFILER_ZONE_END
}


void textf(short x, short y, const char* fmt, ...) {
	PROFILER_ZONE_START

	char buf[256] = {0};
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, 256, fmt, args);
	va_end(args);

	text(buf, x, y);
	PROFILER_ZONE_END
}


// void load_glyph_buffer(char* str) {
// 	u32 point;
// 	while((point = gfx_readutf8((u8**) &str)))
// 		glyph_ids[glyph_cur++] = str[i];


// 	u32 len;
// 	LBT_Glyph* ligated = LBT_apply_chain(glyph_ids, str, glyph_cur, &len);
// 	for(u32 i = 0; i < len; i ++)
// 		glyph_ids[i] = ligated[i];
// 	glyph_cur = len;
// }

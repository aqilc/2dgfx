/*
	SDF Resources:
		TinySDF(this impl): https://github.com/mapbox/tiny-sdf/blob/main/index.js
		SDT Dead Reckoning technique: https://github.com/arkanis/single-header-file-c-libs/blob/master/sdt_dead_reckoning.h
		
		USES SUBPIXEL + SDF: https://github.com/astiopin/webgl_fonts
		Using freetype's destructor to render: https://github.com/mapbox/sdf-glyph-foundry/blob/master/include/mapbox/glyph_foundry_impl.hpp
		SDF after raster: https://github.com/raphm/makeglfont/blob/master/makeglfont.cpp#L214
		
		Custom Font rendering solution in a few lines: https://github.com/raphlinus/font-rs/blob/master/src/font.rs
		
		https://github.com/Chlumsky/msdfgen/blob/master/core/render-sdf.cpp
		https://github.com/behdad/glyphy
		https://stackoverflow.com/questions/25956272/better-quality-text-in-webgl
*/

#include "tests.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vec.h>
#include <hash.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stdbool.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef union gfx_vector {
  struct { int x, y; };
  struct { float fx, fy; };
  struct { int w, h; };
} gfx_vector;

typedef struct gfx_char {
  gfx_vector place;   // Place in texture atlas
  gfx_vector size;    // w, h of char texture
  gfx_vector bearing; // Refer to freetype docs on this, but I need to keep in track of it.
  uint32_t advance;   // How far this character moves forward the text cursor
  unsigned long c;    // The character(unicode)
} gfx_char;


typedef struct gfx_texture {
  uint8_t* tex;    // Pointer to texture's data buffer (RGBA)
  gfx_vector size;
} gfx_texture;


FT_Library ft;
FT_Face face;
gfx_texture tex;


#define GFX_FONT_START_SIZE 24
#define BUFFER (GFX_FONT_START_SIZE / 8)
#define FONTBUFSIZE (GFX_FONT_START_SIZE + BUFFER * 4)
#define INF 1.0e20
#define RADIUS (GFX_FONT_START_SIZE / 3.0)
#define CUTOFF 0.0

double gridOuter[FONTBUFSIZE * FONTBUFSIZE] = {};
double gridInner[FONTBUFSIZE * FONTBUFSIZE] = {};

double f[FONTBUFSIZE] = {};
double z[FONTBUFSIZE + 1] = {};
int v[FONTBUFSIZE] = {};

static inline void edt1d(double* grid, u32 offset, u32 stride, u32 length) {
	v[0] = 0;
	z[0] = -INF;
	z[1] = INF;
	f[0] = grid[offset];
	
	double s = 0;
	for (int q = 1, k = 0; q < length; q++) {
		f[q] = grid[offset + q * stride];
		int q2 = q * q;
		do {
			int r = v[k];
			s = (f[q] - f[r] + (double) (q2 - r * r)) / (q - r) / 2.0;
		} while (s <= z[k] && --k > -1);

		k++;
		v[k] = q;
		z[k] = s;
		z[k + 1] = INF;
	}

	for (int q = 0, k = 0; q < length; q++) {
		while (z[k + 1] < q) k++;
		int r = v[k];
		double qr = q - r;
		grid[offset + q * stride] = f[r] + qr * qr;
	}
}
static inline void edt(double* data, u32 x0, u32 y0, u32 width, u32 height, u32 gridSize) {
	for (u32 x = x0; x < x0 + width; x++)
		edt1d(data, y0 * gridSize + x, gridSize, height);
		
	for (u32 y = y0; y < y0 + height; y++)
		edt1d(data, y * gridSize + x0, 1, width);
}

TEST("Load a font") {

	// Loads the freetype library.
	if(!ft) FT_Init_FreeType(&ft);

	// Loads the new face in using the library
	// if(FT_New_Face(ft, "C:\\Windows\\Fonts\\msyhl.ttc", 1, (FT_Face*) &face)) printf("ooooop couldn't load font");
	if(FT_New_Face(ft, "roboto.ttf", 0, (FT_Face*) &face)) printf("ooooop couldn't load font");
	FT_Set_Pixel_Sizes(face, 0, GFX_FONT_START_SIZE);
}

TEST("Load a") {

	// 泽
	// FT_ULong c = 0x6CFD;
	FT_ULong c = 'T';
	
	FT_Load_Char(face, c, FT_LOAD_RENDER);

	int top = face->glyph->bitmap_top;
	int width = face->glyph->bitmap.width;
	int height = face->glyph->bitmap.rows;

	// Writes the character's pixels to the atlas buffer.
	int x = 0, y = 0;
	int w = width, h = height;
	u8* t = tex.tex + x + y * tex.size.w;
	// for(int i = 0; i < h; i ++)
	// 	memcpy(t + i * tex.size.w, face->glyph->bitmap.buffer + i * w, w);

	u32 bufwidth = width + 2 * BUFFER;
	u32 bufheight = height + 2 * BUFFER;

	u32 len = bufwidth * bufheight;
	u8* data = malloc(len);

	// ctx.clearRect(BUFFER, BUFFER, width, height);
	// ctx.fillText(char, BUFFER, BUFFER + glyphTop);
	u8* imgData = face->glyph->bitmap.buffer;

	// Initialize grids outside the glyph range to alpha 0
	for(u32 i = 0; i < len; i ++)
		gridOuter[i] = INF;
	for(u32 i = 0; i < len; i ++)
		gridInner[i] = 0.0;

	for (u32 y = 0; y < height; y++) {
		for (u32 x = 0; x < width; x++) {
			u8 a = imgData[y * width + x]; // alpha value
			if (a == 0) continue; // empty pixels

			u32 j = (y + BUFFER) * bufwidth + x + BUFFER;

			if (a == 255) { // fully drawn pixels
				gridOuter[j] = 0.0;
				gridInner[j] = INF;
			} else { // aliased pixels
				double d = 0.5 - ((double) a / 255.0);
				gridOuter[j] = d > 0.0 ? d * d : 0.0;
				gridInner[j] = d < 0.0 ? d * d : 0.0;
			}
		}
	}

	edt(gridOuter, 0, 0, bufwidth, bufheight, bufwidth);
	edt(gridInner, BUFFER, BUFFER, width, height, bufwidth);

	for (u32 i = 0; i < len; i++) {
		double d = sqrt(gridOuter[i]) - sqrt(gridInner[i]);
		double res = round(255.0 - 255.0 * (d / RADIUS + CUTOFF));
		if(res > 255.0) data[i] = 255;
		else if (res < 0.0) data[i] = 0;
		else data[i] = res;
		// printf("%d(%d, %d): %f (%f, %f) -> %d (%f)\n", i, i % bufwidth, i / bufwidth, d, gridOuter[i], gridInner[i], data[i], 255.0 * (d / RADIUS + CUTOFF));
	}

	// stbi_write_png("bitmap1.png", tex.size.w, tex.size.h, 1, tex.tex, tex.size.w);
	tex.tex = data;
	tex.size = (gfx_vector) { bufwidth, bufheight };
}

TEST("Write bitmap to disk") {
	stbi_write_png("bitmap2.png", tex.size.w, tex.size.h, 1, tex.tex, tex.size.w);
}

#include "tests_end.h"

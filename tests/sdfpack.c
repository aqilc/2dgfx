#include "tests.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vec.h>
#include <hash.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stdbool.h>


#define GFX_FONT_ATLAS_START_SIZE 256
#define RENDERING_FONT_SIZE(...) 104##__VA_ARGS__

#define BUFFER (RENDERING_FONT_SIZE() / 8)
#define FONTBUFSIZE (RENDERING_FONT_SIZE() + BUFFER * 4)
#define INF 1e20
#define RADIUS (RENDERING_FONT_SIZE(.0) / 3.0)
#define CUTOFF 0.25

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

typedef struct gfx_typeface {
  char* name;
  uint32_t space_width;
  gfx_texture* tex;                      // Texture atlas
  struct gfx_atlas_node* atlas_tree; // Vec<gfx_atlas_node>
  void* face;                        // FT_Face internally
  bool added;
  ht(unsigned long, gfx_char) chars;
} gfx_typeface;

typedef gfx_typeface* typeface;

struct gfx_atlas_node {
  gfx_vector p; // Place
  gfx_vector s; // Size
  int child[2];
  bool filled;
};

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
		if (prnt->p.x + prnt->s.w == INT_MAX) realsize.w = face->tex->size.w - prnt->p.x;
		if (prnt->p.y + prnt->s.h == INT_MAX) realsize.h = face->tex->size.h - prnt->p.y;
		
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
			s = (f[q] - f[r] + q2 - r * r) / (q - r) / 2;
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

// ------------------------------------------------------------------------ STUB FUNCTIONS FROM GFX --------------------------------------------------------------------------

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

FT_Library ft;
typeface face;

double gridOuter[FONTBUFSIZE * FONTBUFSIZE] = {};
double gridInner[FONTBUFSIZE * FONTBUFSIZE] = {};


static void gfx_loadfontchar(FT_ULong c) {
	FT_Load_Char(face->face, c, FT_LOAD_RENDER);

	u32 width = ((FT_Face) face->face)->glyph->bitmap.width;
	u32 height = ((FT_Face) face->face)->glyph->bitmap.rows;
	u32 bufwidth = width + 2 * BUFFER;
	u32 bufheight = height + 2 * BUFFER;

	// Tries to add the character, resizing the whole texture until it's done
	struct gfx_atlas_node* maybe;
	gfx_vector size = { bufwidth, bufheight };
	u32 growth = 1;
	while(!(maybe = gfx_atlas_insert(face, 0, &size))) {
		printf("Atlas size doubled for font '%s'", face->name);
		growth *= 2, face->tex->size.w *= 2, face->tex->size.h *= 2;
	}

	if(!face->tex->tex)
		face->tex->tex = malloc(face->tex->size.w * face->tex->size.h * sizeof(u8));
	else if(growth > 1) {
		u32 ow = face->tex->size.w / growth, oh = face->tex->size.h / growth;
		u8* ntex = malloc(face->tex->size.w * face->tex->size.h * sizeof(u8));
		u8* otex = face->tex->tex;
		
		for(int i = 0; i < oh; i ++)
			memcpy(ntex + i * face->tex->size.w, otex + i * ow, ow);

		face->tex->tex = ntex;
		free(otex);
	}
	
	u32 len = bufwidth * bufheight;

	// Initialize grids outside the glyph range to alpha 0
	for(u32 i = 0; i < len; i ++)
		gridOuter[i] = INF;
	memset(gridInner, 0, sizeof(gridInner));
	memset(f, 0, sizeof(f));
	memset(z, 0, sizeof(z));
	memset(v, 0, sizeof(v));

	for (u32 y = 0; y < height; y++) {
		for (u32 x = 0; x < width; x++) {
			u8 a = ((FT_Face) face->face)->glyph->bitmap.buffer[y * width + x]; // alpha value
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

	int x = maybe->p.x, y = maybe->p.y;
	int tw = face->tex->size.w, th = face->tex->size.h;
	u8* letter = face->tex->tex + x + y * tw;
	for (u32 i = 0; i < len; i++) {
		double res = 255.0 - 255.0 * ((sqrt(gridOuter[i]) - sqrt(gridInner[i])) / RADIUS + CUTOFF);
		if(res >= 255.0) letter[i % bufwidth + (i / bufwidth) * tw] = 255;
		else if (res <= 0.0) letter[i % bufwidth + (i / bufwidth) * tw] = 0;
		else letter[i % bufwidth + (i / bufwidth) * tw] = res;
	}
	
	face->added = true;
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
}

void gfx_loadfont(const char* file) {

	gfx_typeface font = {
		.name = malloc(strlen(file) + 1),
		.atlas_tree = vnew(),
		.tex = malloc(sizeof(gfx_texture)),
		.chars = {0},
		.face = NULL
	};
	memcpy(font.name, file, strlen(file) + 1);
	memcpy(font.tex, &(gfx_texture) {
		.size = { GFX_FONT_ATLAS_START_SIZE, GFX_FONT_ATLAS_START_SIZE }
	}, sizeof(gfx_texture));
	vpush(font.atlas_tree, { .s = { INT_MAX, INT_MAX } });
	
	// Loads the freetype library.
	if(!ft) FT_Init_FreeType(&ft);

	// Loads the new face in using the library
	if(FT_New_Face(ft, file, 0, (FT_Face*) &font.face)) printf("ooooop couldn't load font");
	FT_Set_Pixel_Sizes(font.face, 0, RENDERING_FONT_SIZE());

	// Adds space_width
	FT_Load_Char(font.face, ' ', FT_LOAD_RENDER);
	font.space_width = ((FT_Face) font.face)->glyph->advance.x >> 6;

	// Stores the font
	face = malloc(sizeof(gfx_typeface));
	memcpy(face, &font, sizeof(gfx_typeface));
}

void text(const char* str) {
	FT_ULong point;
	while ((point = gfx_readutf8((u8**) &str))) {
		if (point == '\n' || point == ' ') continue;
		
		gfx_char* ch = hget(face->chars, point);
		if(!ch) gfx_loadfontchar(point);
		else printf("'%c': %d, %d\n", (char) ch->c, ch->place.w, ch->place.h);
	}
}



TEST("Load a font") {
	gfx_loadfont("roboto.ttf");
}

TEST("Load some characters") {
	text("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890,.()[]~`!@^#$%&*|\\/-_><\"';:+=?");
}

TEST("Write bitmap to disk") {
	stbi_write_png("bitmap.png", face->tex->size.w, face->tex->size.h, 1, face->tex->tex, face->tex->size.w);
}

#include "tests_end.h"

#include "tests.h"
#include <2dgfx.h>

gfx_face fon;
gfx_img hi;
TEST("startup") {
	gfx_init("test window", NULL);
}

TEST("load font") {
	fon = gfx_load_font("roboto.ttf");
}

TEST("load image") {
	hi = gfx_load_img("hyperAngery.png");
}

TEST("disregard") {
	gfx_frame();
}

TEST("rectangle") {
	fill(255, 255, 255, 255);
	rect(100, 100, 200, 200);
}

TEST("Image display") {
	image(hi, 300, 200, 100, 100);
}
TEST("Image display AGAIN") {
	image(hi, 450, 200, 100, 100);
}

TEST("first text call") {
	font_size(20);
	text("ABCDEFGHIJKLMNOP", 20, 350);
}

TEST("second text call") {
	text("ABCDEFGHIJKLMNOP", 20, 370);
}


#include "tests_end.h"

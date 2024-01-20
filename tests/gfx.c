#include "tests.h"
#include <2dgfx.h>

typeface fon;
img hi;
TEST("startup") {
	gfx_init("test window", 800, 600);
}

TEST("load font") {
	fon = gfx_loadfont("roboto.ttf");
}

TEST("load image") {
	hi = gfx_loadimg("hyperAngery.png");
}

TEST("disregard") {
	gfx_nextframe();
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
	fontsize(20);
	text("ABCDEFGHIJKLMNOP", 20, 350);
}

TEST("second text call") {
	text("ABCDEFGHIJKLMNOP", 20, 370);
}

TEST("frame end") {
	gfx_frameend();
}

#include "tests_end.h"

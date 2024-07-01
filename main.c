#include "lib/2dgfx.h"
#include <stdlib.h>
#include <math.h>

// My custom graphics lib :3
#define GFX_MAIN_DEFINE
#include <2dgfx.h>
#include <vec.h>


char* messages[] = {
  "lol",
  "ez",
  "cool demo",
  "but CAN YOU DO THIS?",
  "send n*des",
  "lmao"
};

#define MLEN (sizeof(messages) / sizeof(messages[0]))

struct text_particle {
  gfx_vector pos;
  int size;
  int text;
  float spd;
  unsigned char col[3];
} particles[5000];

#define PLEN (sizeof(particles) / sizeof(particles[0]))

void particle_reset(int id, int height) {
  particles[id].pos.fx = -(rand() / (float) RAND_MAX) * 300 + 100;
  particles[id].spd    =  (rand() / (float) RAND_MAX) * 5.f + 1.f;
  particles[id].pos.y  =  (rand() / (float) RAND_MAX) * (height + 50);
  particles[id].size   =  (rand() / (float) RAND_MAX) * 45 + 5;
  particles[id].text   =  (rand() % MLEN);
  particles[id].col[0] =  (rand() / (float) RAND_MAX) * 200 + 55;
  particles[id].col[1] =  (rand() / (float) RAND_MAX) * 200 + 55;
  particles[id].col[2] =  (rand() / (float) RAND_MAX) * 200 + 55;
}
void particles_draw(int width, int height) {
  for(int i = 0; i < PLEN; i ++) {
    if(particles[i].pos.fx > width + 100) particle_reset(i, height);
    particles[i].pos.fx += particles[i].spd;
    font_size(particles[i].size);
    fill(particles[i].col[0], particles[i].col[1], particles[i].col[2], 255);
    text(messages[particles[i].text], particles[i].pos.fx, particles[i].pos.y);
  }
}

gfx_img hi;
gfx_face fon;
char* fps = NULL;
void setup() {
  gfx_init("2dgfx", 800, 600);
  hi = gfx_loadimg("hyperAngery.png");
  fon = gfx_load_font("roboto.ttf");

  for(int i = 0; i < PLEN; i ++)
    particle_reset(i, 600);
}
void loop() {
  gfx_vector m = gfx_mouse();
  gfx_vector dims = gfx_screen_dims();


  // particles_draw(dims.w, dims.h);
  fill(255, 255, 255, 255);
  rect(m.x, m.y, 20, 20);
  image(hi, 350, 200, 100, 100);
  font_size(20);
  text("hello! THIS IS FINALLY WORKING PROPERLY WOOOOOO eahubrancodikr", 10, 400);
  text("AAAAAAAAAAAA YES YOU DON'T DISTORT ANYMOER", 10, 430);
  font_size(200);
  text("BIG", 50, 300);
  // circle(400, 300, 200);

  gfx_default_fps_counter();
}

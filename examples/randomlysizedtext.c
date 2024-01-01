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
  unsigned char col;
} particles[500];

#define PLEN (sizeof(particles) / sizeof(particles[0]))

void particle_reset(int id, int height) {
  particles[id].pos.fx = -(rand() / (float) RAND_MAX) * 300 + 100;
  particles[id].spd    =  (rand() / (float) RAND_MAX) * 5.f + 1.f;
  particles[id].pos.y  =  (rand() / (float) RAND_MAX) * (height + 50);
  particles[id].size   =  (rand() / (float) RAND_MAX) * 45 + 5;
  particles[id].text   =  (rand() % MLEN);
  particles[id].col    =  (rand() / (float) RAND_MAX) * 200 + 55;
}
void particles_draw(int width, int height) {
  for(int i = 0; i < PLEN; i ++) {
    if(particles[i].pos.fx > width + 100) particle_reset(i, height);
    particles[i].pos.fx += particles[i].spd;
    fontsize(particles[i].size);
    fill(particles[i].col, particles[i].col, particles[i].col, 255);
    text(messages[particles[i].text], particles[i].pos.fx, particles[i].pos.y);
  }
}

img hi;
typeface fon;
char* fps = NULL;
void setup() {
  gfx_init("2dgfx", 800, 600);
  hi = gfx_loadimg("hyperAngery.png");
  fon = gfx_loadfont("roboto.ttf");

  for(int i = 0; i < PLEN; i ++)
    particle_reset(i, 600);
}
void loop() {
  gfx_vector m = gfx_mouse();
  gfx_vector dims = gfx_screen_dims();


  particles_draw(dims.w, dims.h);
  fill(255, 255, 255, 255);
  rect(m.x, m.y, 20, 20);
  fontsize(200);
  text("BIG", 50, 300);


  if(gfx_fpschanged())
    fps = vfmt("%.2f fps", gfx_fps());
  fontsize(20);
  if(fps) text(fps, 800 - 150, 600 - 10);
}

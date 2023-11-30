
#include <stdio.h>

// My custom graphics lib :3
#include <2dgfx.h>
#include <vec.h>

int main(void) {
  gfx_init("2dgfx", 800, 600);
  img hi = gfx_loadimg("hyperAngery.png");
  typeface fon = gfx_loadfont("Roboto-Medium.ttf");
  char* fps = NULL;
  
  while(gfx_nextframe()) {
    gfx_vector m = gfx_mouse();
    fill(255, 255, 255, 255);
    rect(m.x, m.y, 20, 20);
    image(hi, 350, 200, 100, 100);
    fontsize(20);
    text("hello! THIS IS FINALLY WORKING PROPERLY WOOOOOO eahubrancodikr", 10, 400);
    text("AAAAAAAAAAAA YES YOU YOU DON'T DISTORT ANYMOER", 10, 430);


    if(gfx_fpschanged())
      fps = vfmt("%.2f fps", gfx_fps());
    if(fps) text(fps, 800 - 150, 600 - 10);
    
    gfx_frameend();
  }

  return 0;
}


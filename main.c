
#include <stdio.h>

// My custom graphics lib :3
#include <2dgfx.h>
#include <vec.h>

int main(void) {

  // https://www.glfw.org/docs/3.0/window.html#window_fbsize
  // glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  gfx_init("2dgfx", 800, 600);
  img hi = gfx_loadimg("hyperAngery.png");
  typeface fon = gfx_loadfont("Roboto-Medium.ttf");
  char* fps;
  
  while(gfx_nextframe()) {

    gfx_vector m = gfx_mouse();
    fill(255, 255, 255, 255);
    rect(m.x, m.y, 20, 20);
    image(hi, 350, 200, 100, 100);
    fontsize(20);
    // text("h", 100, 400);
    text("hello! THIS IS FINALLY WORKING PROPERLY WOOOOOO eahubrancodikr", 10, 400);
    text("AAAAAAAAAAAA YES YOU YOU DON'T DISTORT ANYMOER", 10, 430);

    // printf("%.2f  ", gfx_fps());
    if(gfx_fpschanged())
      fps = vfmt("%.4f fps", gfx_fps());
    text(fps, 800 - 200, 600 - 10);
    
    //glfwSetWindowShouldClose(window, 1);
    gfx_frameend();
  }

  return 0;
}


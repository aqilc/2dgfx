
#include <stdio.h>

// GLEW, for opengl functions
#define GLEW_STATIC
#include <GL/glew.h>

// Window management
#include <GLFW/glfw3.h>


// My custom graphics lib :3
#include <2dgfx.h>

// void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
// void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

// void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

GLFWwindow* window;

int main(void) {

  /* Initialize the library */
  if (!glfwInit())
    return -1;
  
  // GLFW hints
  // glfwWindowHint(GLFW_DECORATED, GL_FALSE);
  // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  
  /* Create a windowed mode window and its OpenGL context */
  window = glfwCreateWindow(800, 600, "Hello World", NULL, NULL);
  if (!window) {
    puts("couldn't initialize window???");
    glfwTerminate(); return -1; }
  /* Make the window's context current */
  glfwMakeContextCurrent(window);
  
  // Set the framerate to the framerate of the screen, basically 60fps.
  glfwSwapInterval(1);
  
  // Initialize GLEW so we can reference OpenGL functions.
  if(glewInit()/* != GLEW_OK */) {
    printf("error with initializing glew");
    glfwTerminate(); return -1; }

  // GLFW input callbacks
  // glfwSetMouseButtonCallback(window, mouse_button_callback);
  // glfwSetCursorPosCallback(window, cursor_position_callback);
  // glfwSetKeyCallback(window, key_callback);

  gfx_init(800, 600);
  img hi = gfx_loadimg("hyperAngery.png");

  while(!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    fill(255, 255, 255, 255);
    rect(100, 100, 200, 200);
    image(hi, 200, 200, 100, 100);
    draw();
    
    //glfwSetWindowShouldClose(window, 1);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  return 0;
}


#include "tests.h"
#include "linmath.h"

#define GLEW_STATIC
#include "GL/glew.h"

#include "GLFW/glfw3.h"


void GLAPIENTRY glDebugMessageHandler(GLenum source,
		GLenum type, GLuint id, GLenum severity, GLsizei length,
		const GLchar* message, const void* userParam) {
	printf("        OpenGL Error: %s (Code: %d)\n", message, type);
}

#define GLSL(...) "#version 330 core\n" #__VA_ARGS__
static struct {
	const char* vert1;
	const char* frag1;
} default_shaders = {
	.vert1 = GLSL(
		precision mediump float;
		layout (location = 0) in vec4 pos;
		out vec4 v_pos;
		out float id;
		
		void main() {
			gl_Position = pos;
			v_pos = pos;
			id = float(gl_VertexID % 3);
		}
	),

	.frag1 = GLSL(
		precision mediump float;
		layout (location = 0) out vec4 color;
		in vec4 v_pos;
		in float id;
		void main() {
			color = vec4(ceil(v_pos.x*10)/10.0, ceil(v_pos.y*10)/10.0, id / 3.0, 1.0);
		}
	)
};

#define TRIANGLE_NUM 6
vec2 pts[] = {
  { -1,  1 },
  {  1,  1 },
  {  1, -1 },
  { -1, -1 },
  { -1,  1 },
  {  1, -1 },
};

TEST("Display") {
  GLFWwindow* window;
	glfwInit();
 
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	// glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	// glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
 
	// Create a windowed mode window and its OpenGL context
	window = glfwCreateWindow(400, 400, "test", NULL, NULL);
	glfwMakeContextCurrent(window);
	
	// glfwSwapInterval(1);
	
	glewInit();
	glDebugMessageCallback(glDebugMessageHandler, NULL);
 
	// Enables OpenGL Features
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	// glEnable(GL_DEPTH_TEST);
 
	GLuint prog1 = glCreateProgram();
 
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &default_shaders.vert1, NULL);
	glCompileShader(vs);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &default_shaders.frag1, NULL);
	glCompileShader(fs);
	
	glAttachShader(prog1, vs);
	glAttachShader(prog1, fs);
	glLinkProgram(prog1);
	glValidateProgram(prog1);
 
	glDetachShader(prog1, vs);
	glDetachShader(prog1, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
 
	GLuint vao1;
	glGenVertexArrays(1, &vao1);
	
	glBindVertexArray(vao1);
	glUseProgram(prog1);
 
	GLuint vb;
	glGenBuffers(1, &vb);
	glBindBuffer(GL_ARRAY_BUFFER, vb);
	
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, 0);
	
	glBufferData(GL_ARRAY_BUFFER, TRIANGLE_NUM * 2 * 4, pts, GL_STATIC_DRAW);
	
	while(!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, TRIANGLE_NUM);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}

#include "tests_end.h"
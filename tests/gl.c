
#include <stdio.h>

// GLEW, for opengl functions
#define GLEW_STATIC
#include <GL/glew.h>

// Window management
#include <GLFW/glfw3.h>

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
		layout (location = 1) in vec2 uv;
		layout (location = 2) in vec4 col;

		uniform mat4 u_mvp;

		out vec2 v_uv;
		out vec4 v_col;

		void main() {
			gl_Position = u_mvp * pos;
			v_uv = uv;
			v_col = col;
		}
	),

	.frag1 = GLSL(
		precision mediump float;
		layout (location = 0) out vec4 color;

		in vec2 v_uv;
		in vec4 v_col;

		uniform sampler2D u_tex;
		uniform int u_shape;

		vec4 text;
		float a1;
		float a2;

		void main() {
			if(u_shape == 0) {
				if(v_uv.x < 0) {
					color = v_col; // Shape rendering
					return;
				}
				color = vec4(v_col.rgb, texture(u_tex, v_uv).r * v_col.a); // Text rendering
			}
			else {
				text = texture(u_tex, v_uv);
				color = vec4(text.rgb, text.a * v_col.a); // Image rendering
			}
		}
	)
};

int main() {
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
	window = glfwCreateWindow(600, 200, "test", NULL, NULL);
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

	while(!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);


		
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
}

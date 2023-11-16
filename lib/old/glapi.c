
#include <stdio.h>
#include <stdlib.h>
#include "glapi.h"

GLuint cur_va = 0;
GLuint cur_vb = 0;
GLuint cur_ib = 0;
GLuint cur_prog = 0;
drawcontext* cur_ctx = NULL;

GLuint* create_va(uint8_t n) {
  GLuint* a = (GLuint*) malloc(n * sizeof(GLuint));
  glGenVertexArrays(n, a);
  binda(*a);
  return a;
}

GLuint create_vb(void* data, unsigned int size) {
  GLuint b;
  glGenBuffers(1, &b);
  bindv(b);
  glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
  return b;
}

GLuint create_ib(uint16_t* data, unsigned int size) {
  GLuint b;
  glGenBuffers(1, &b);
  bindi(b);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * sizeof(short), data, GL_DYNAMIC_DRAW);
  return b;
}

// Binding methods
void binda(GLuint id) { glBindVertexArray(id); cur_va = id; cur_vb = 0; cur_ib = 0; }
void bindv(GLuint id) { glBindBuffer(GL_ARRAY_BUFFER, id); cur_vb = id; }
void bindi(GLuint id) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id); cur_ib = id; }
void bindp(GLuint id) { glUseProgram(id); cur_prog = id; }

void activet(GLenum slot) { glActiveTexture(GL_TEXTURE0 + slot); }
void bindt(GLuint id) { glBindTexture(GL_TEXTURE_2D, id); }

unsigned short getsize(GLenum type) {
  switch(type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
      return 1;
    default: return 4;
  }
}

void lpush(vlayout* l, GLuint type, unsigned char count, bool normalized) {
  struct ltype** cur = &l->types;
  while (*cur != NULL) cur = &(*cur)->next;
  (*cur) = (struct ltype*) calloc(sizeof(struct ltype), 1);
  (*cur)->type = type;
  (*cur)->count = count;
  (*cur)->normalized = normalized;
}
void lpushf(vlayout* l, unsigned char count) { l->stride += 4 * count; lpush(l, GL_FLOAT, count, false); }
// void lpushc(vlayout* l, unsigned char count) { l->stride += 1 * count; lpush(l, GL_UNSIGNED_BYTE, count, true); }
void lpushi(vlayout* l, unsigned char count) { l->stride += 4 * count; lpush(l, GL_INT, count, false); }

void lapply(vlayout* l) {
  struct ltype* cur = l->types;
  unsigned int i = 0, offset = 0;
  while(cur != NULL) {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, cur->count, cur->type, cur->normalized ? GL_TRUE : GL_FALSE, l->stride, (const void*) offset);
    offset += cur->count * getsize(cur->type); i++;
    cur = cur->next;
  }
}

// Complicated function for getting uniform location thru cache
GLint getloc(char* name) {
  
  // There's no point if there's no program bound anyways
  if(!cur_prog) return -1;
  
  // Gets the location from cache or sets it in cache
  GLint* v = (GLint*) hget(*cur_ctx->u, name);
  if(v == NULL)
    hsets(*cur_ctx->u, name) = glGetUniformLocation(cur_prog, name);
  return *v;
}

void setum4(char* name, mat4x4 p) { glUniformMatrix4fv(getloc(name), 1, GL_FALSE, (const GLfloat*) p); }
void setui(char* name, unsigned int p) { glUniform1i(getloc(name), p); }
void setu3f(char* name, vec3 p) { glUniform3f(getloc(name), p[0], p[1], p[2]); }
void setu2f(char* name, vec2 p) { glUniform2f(getloc(name), p[0], p[1]); }

void setcontext(drawcontext* c) { cur_ctx = c; bindp(c->pid); binda(c->vaid); }
void initcontext(drawcontext** d, char* shaderfile) {
  (*d) = (drawcontext*) calloc(sizeof(drawcontext), 1);
  (*d)->pid = create_p(shaderfile);
  (*d)->u = calloc(1, sizeof(ht(char*, GLuint)));
  (*d)->vaid = *create_va(1);
  cur_ctx = *d;
}

// Coordinate System -> Screen 1:1 Mapping
void coords_screen() {
  if(cur_prog == 0) return;
  mat4x4 p;
  // Fills in p?
  mat4x4_identity(p);
  
  // Creates a projection to screencoords
  mat4x4_ortho(p, .0f, (float) g.width, (float) g.height, .0f, 1.0f, -1.f);
  setum4("u_mvp", p);
}

GLuint texture(unsigned char* buf, int width, int height, GLenum format, bool typeface) {
  GLuint t;
  
  // Generates and binds the texture
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  
  // Sets default params
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Sends texture data to the gpu
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, buf);


  // We just don't need mipmaps for fonts so we do this for normal images
  if(!typeface) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  
  return t;
}

GLuint pixpertex(unsigned char* buf, int width, int height, GLenum format) {
  GLuint t;
  
  // Generates and binds the texture
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  
  // Sets default params
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Sends texture data to the gpu
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, buf);
  
  return t;
}

// Magic things, idk wtf this code does
unsigned int compileshader(unsigned int type, const char* src) {
  unsigned int id = glCreateShader(type);
  glShaderSource(id, 1, &src, NULL);
  glCompileShader(id);
  
  int res;
  glGetShaderiv(id, GL_COMPILE_STATUS, &res);
  
  if(res == GL_FALSE) {
    int length; glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    char* message = (char*) malloc(length * sizeof(char));
    glGetShaderInfoLog(id, length, &length, message);
    printf("Failed to compile %s shader!\n%s\n", (type == GL_VERTEX_SHADER ? "vertex" : "fragment"), message);
    glDeleteShader(id); return 0;
  }
  
  return id;
}

// same as the last function
unsigned int create_p(char* filename) {
  char* contents = read(filename);
  
  // Splits the string where the `# frag` occurs in the file with just nullbytes, gonna just hope it works
  char* split = strstr(contents, "# frag");
  *(split-1) = '\0';
  //*(split+6) = '\0';
  
  // Gets the fragment shader by skipping over the '# frag'
  char* frag = split + 6;

  unsigned int program = glCreateProgram(),
  
    // Vertex Shader
    vs = compileshader(GL_VERTEX_SHADER, contents),
    
    // Fragment Shader
    fs = compileshader(GL_FRAGMENT_SHADER, frag);
  
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glValidateProgram(program);

  glDetachShader(program, vs);
  glDetachShader(program, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);
  
  // Frees up file strings (it frees both dw)
  free(contents);
  
  bindp(program);
  return program;
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
  printf("[OpenGL Error](%d) %s\n", type, message);
}

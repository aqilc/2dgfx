#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include "vec.h"

union vec {
  struct { int x; int y; };
  struct { int w; int h; };
};
typedef struct pvec { double x, y; } pvec;


typedef struct CharNode {
  union vec p; // Place
  union vec s; // Size
  struct CharNode* child[2];
  bool filled;
} CharNode;
CharNode* charnode_insert(CharNode* pnode, union vec* size);
void charnode_free(CharNode* root);

typedef struct gamestate {
  double delta; // Delta time in the last frame
  double last;
  unsigned int frames;
  int width, height; // Starting width and height of window
} gamestate;


extern gamestate g;

char* read(char* file);
void* read_image(char *file, uint32_t *width, uint32_t *height);
#endif


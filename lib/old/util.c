
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <spng/spng.h>
#include <util.h>

// Unperformant file reader, but it works ok :(
char* read(char* file) {
  FILE *fp = fopen(file, "rb");
  if(!fp) {
    printf("Couldn't open file %s", file);
    return "";
  }
  
  // Finds length of file
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  rewind(fp);
  
  // Mallocs enough for the file + nullbyte i think
  char* contents = malloc(len * sizeof(char) + 1);
  contents[len] = '\0';
  
  // Reads file into the contents variable then closes it
  fread(contents, 1, len, fp);
  fclose(fp);
  
  // returns both
  return contents;
}

size_t read_size(char* file, void** buf) {
  FILE *fp = fopen(file, "rb");
  if (!fp) {
    printf("Couldn't open file %s", file);
    return 0;
  }

  // Finds length of file
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  rewind(fp);

  // Mallocs enough for the file + nullbyte i think
  char *contents = malloc(len * sizeof(char) + 1);
  contents[len] = '\0';

  // Reads file into the contents variable then closes it
  fread(contents, 1, len, fp);
  fclose(fp);

  *buf = contents;

  // returns both
  return len;
}

  void *read_image(char *file, uint32_t *width, uint32_t *height)
  {
    spng_ctx *ctx;
    struct spng_ihdr ihdr;
    void *image = NULL;
    size_t size;

    void *buf;
    size_t bufsize = read_size(file, &buf);

    int error;

    ctx = spng_ctx_new(0);
    if (ctx == NULL)
      return NULL;

    // Sets buffer, reads header, and sets the size of the output image
    if ((error = spng_set_png_buffer(ctx, buf, bufsize)))
      goto err;
    if ((error = spng_get_ihdr(ctx, &ihdr)))
      goto err;
    if ((error = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &size)))
      goto err;

    image = malloc(size);
    if (image == NULL)
      goto err;
    if ((error = spng_decode_image(ctx, image, size, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS)))
      goto err;

    *width = ihdr.width;
    *height = ihdr.height;

    return image;

  err:
    if (image != NULL)
      free(image);
    puts(spng_strerror(error));
    spng_ctx_free(ctx);
    return NULL;
}

CharNode* charnode_insert(CharNode* pnode, union vec* size) {
  if(pnode->child[0] == NULL || pnode->child[1] == NULL) {
    
    // If the node is already filled
    if(pnode->filled) return NULL;
    
    // If the image doesn't fit in the node
    if(pnode->s.x < size->x || pnode->s.y < size->y) return NULL;
    
    // If the image fits perfectly in the node, return the node since we can't really split anymore anyways
    if(pnode->s.x == size->x && pnode->s.y == size->y) { pnode->filled = true; return pnode; }
    
		// Splits node
		CharNode* c1 = calloc(1, sizeof(CharNode));
		CharNode* c2 = calloc(1, sizeof(CharNode));
		pnode->child[0] = c1;
		pnode->child[1] = c2;
		
		int dw = pnode->s.x - size->x;
		int dh = pnode->s.y - size->y;
		
		// Sets everything that would be the same for both type of splits
		c1->p.x = pnode->p.x;
		c1->p.y = pnode->p.y;

		// If the margin between the right of the rect and the edge of the parent rectangle is larger than the margin of the bottom of the rect and the bottom of the parent node
		if(dw > dh) {
      // Vertical Split
      c1->s.x = size->x; // - 1
      c1->s.y = pnode->s.y;
      
      c2->p.x = pnode->p.x + size->x;
      c2->p.y = pnode->p.y;
      c2->s.x = pnode->s.x - size->x;
      c2->s.y = pnode->s.y;
      
		} else {
      // Horizontal Split
      c1->s.x = pnode->s.x;
      c1->s.y = size->y; // - 1
      
      c2->p.x = pnode->p.x;
      c2->p.y = pnode->p.y + size->y;
      c2->s.x = pnode->s.x;
      c2->s.y = pnode->s.y - size->y;
    }
    
    return charnode_insert(c1, size);
  }
  
  CharNode* firstchildres = charnode_insert(pnode->child[0], size);
  if(firstchildres == NULL)
    return charnode_insert(pnode->child[1], size);
  return firstchildres;
}

// Frees the whole charnode linked list
void charnode_free(CharNode* root) {
  if(!root) return;

  // Frees both children and then itself
  charnode_free(root->child[0]);
  charnode_free(root->child[1]);
  free(root);
}

/*
 * IDEAS:
  * I really hate that we have to store pointers to linked lists in every item, even when we require that 3/4s of the actual buffer be non-conflicts. It is wasting wasting 1/3rd of the memory for 3/4ts of the items, coming up to more than 1/4th of the whole hashtable being completely useless.
    What if we just remove the pointer to the next item(basically take out the linked list) and just put the item in conflict in the next row? We can still count conflicts, and although they would be more prevalent(since one conflict can now become another conflict), it would be much better for cache locality and memory usage.
   * This idea deviates from the idea of a hashtable completely, and I realize it's more like just using a hash function to kind of tell where something is lol. Also, it could be worse for perf but who knows, string comp is pretty fast and cache locality is important.
   [ ] Take off the `next` pointer
   [ ] Implement putting items beside the original bucket if there is a conflict
   [ ] Get items through checking every string key in conflict.
   [ ] Manage resizing if the item does not fit.
   [ ] Find a way to not loop till the end of the array and checking every key till then.
 * TODO:
  [-] Properly add:
   [x] `keys`
   [x] `filledbuckets`
    [x] In Resize
  [ ] Clearing
  [x] Resizing
   [x] Don't rehash + traverse for keys in the array, just use ptr
  [x] Freeing
*/

#include "hash.h"
#include "hashfunc.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// #include <stdio.h>

typedef unsigned int uint; // bc why

// Aligns to the next multiple of a, where a is a power of 2
static inline uint align(uint n, uint a) { return (n + a - 1) & ~(a - 1); }
static inline void pushkey(struct GENERIC_TABLE_* t, void* ptr) {
  if(!t->keys) {
    t->keys = malloc(10 * sizeof(void*));
    t->keys[0] = ptr;
    t->ksize = 10;
    t->n = 1;
    return;
  }
  uint newsize = align(t->n + 1, 8) + 2;
  if(newsize > t->ksize)
    t->keys = realloc(t->keys, newsize * sizeof(void*)), t->ksize = newsize;
  t->keys[t->n] = ptr;
  t->n ++;
}





// Initializes a hashtable
void htinit(struct GENERIC_TABLE_* t, uint size) {
  t->n = 0; t->size = size;
  t->items = calloc(sizeof(struct GENERIC_ITEM_), size); 
}

// Resizes a hashtable, rehashing all the keys. Slow right now, will make it faster later™.
#define RESIZE(...) {\
  if(!t->size) { htinit(t, size); return true; }\
  struct GENERIC_ITEM_* new = calloc(sizeof(struct GENERIC_ITEM_), size);\
  t->filledbuckets = 0;\
\
  for(uint i = 0; i < t->n; i++) {\
    __VA_ARGS__\
    uint h = hash(*t->keys[i], ksize);\
    uint newind = h % size;\
\
    struct GENERIC_ITEM_* item = (struct GENERIC_ITEM_*)t->keys[i];\
    struct GENERIC_ITEM_* newitem = new + newind;\
\
    /* If it's a top level item, we would have to copy it over or allocate it separately */\
    if (item >= t->items && item < t->items + t->size) {\
      if (newitem->k) {\
        while(newitem->next) newitem = newitem->next;\
        newitem->next = malloc(sizeof(struct GENERIC_ITEM_));\
        newitem = newitem->next;\
      } else t->filledbuckets ++; /* Need to know the new bucket count so we can resize accurately in the future. Biggest risk is causing subsequent resizes, but it would be very rare */\
      newitem->k = item->k;\
      newitem->v = item->v;\
      newitem->next = NULL;\
      t->keys[i] = &newitem->k;\
      continue;\
    }\
\
    /* If it's not top level, it's separately allocated, so we need to free it in the instance we're setting to top level, or just set the item -> next to the new item */\
    if (newitem->k) {\
      while(newitem->next) newitem = newitem->next;\
      newitem = (newitem->next = item);\
      newitem->next = NULL;\
    }\
    else {\
      newitem->k = item->k;\
      newitem->v = item->v;\
      /* newitem->next = NULL; // Already NULL*/\
      t->filledbuckets ++;\
      free(item);\
    }\
    t->keys[i] = &newitem->k;\
  }\
\
  free(t->items);\
  t->items = new;\
  t->size = size;\
  return true;\
}\

bool htresize(struct GENERIC_TABLE_* t, uint size, uint ksize) RESIZE()
bool htsresize(struct GENERIC_TABLE_* t, uint size)
RESIZE(
  uint ksize = strlen(*t->keys[i]) + 1;
)





// Frees a linked item, an item not in the main array of items.
static inline void freeitem(struct GENERIC_ITEM_* item) {
  free(item->k); free(item->v);
  if(item->next) freeitem(item->next);
  free(item);
}

void htfree(struct GENERIC_TABLE_* t) {
  for(uint i = 0; i < t->n; i ++) {
    struct GENERIC_ITEM_* k = (struct GENERIC_ITEM_*) t->keys[i];
    if(k < t->items || k > t->items + t->size) continue; // Only select top level items, items that aren't separately allocated and linked to
    free(k->k); free(k->v);
    if(k->next) freeitem(k->next); // Much simpler to do it recursively here.
  }
  free(t->items);
  free(t->keys);
}

void htreset(struct GENERIC_TABLE_* t) {
  for(uint i = 0; i < t->n; i ++) {
    struct GENERIC_ITEM_* k = (struct GENERIC_ITEM_*) t->keys[i];
    if(k < t->items || k > t->items + t->size) continue;
    free(k->k); free(k->v);
    if(k->next) freeitem(k->next);
    memset(k, 0, sizeof(struct GENERIC_ITEM_));
  }
  memset(t->keys, 0, t->n * sizeof(void*));
  t->n = 0;
  t->filledbuckets = 0;
  // t->size = 0;
}

// Gets an arbitrary type key from hash table
void* htget(struct GENERIC_TABLE_* t, void* k, uint ksize, bool str) {
  if(!t->size) return NULL; // If the table is empty, return NULL (no key found)
  int index = hash(k, ksize) % t->size;
  struct GENERIC_ITEM_* item = t->items + index;
  
  if(item->k != NULL) {
		switch (ksize) {
		case 4:
			do {
				if (*(unsigned int*) item->k == *(unsigned int*) k)
					return item->v;
				else item = item->next;
			} while (item != NULL);
			break;
		case 8:
			do {
				if (*(unsigned long*) item->k == *(unsigned long*) k)
					return item->v;
				else item = item->next;
			} while (item != NULL);
			break;
		default:
			if(str) do {
				if (strcmp(item->k, k) == 0)
					return item->v;
				else item = item->next;
			} while (item != NULL);
			else do {
				if (memcmp(item->k, k, ksize) == 0)
					return item->v;
				else item = item->next;
			} while (item != NULL);
		}
  }

  return NULL;
}

// Sets/inserts an arbitrary type key in the given hash table
void* htset(struct GENERIC_TABLE_* t, void* k, uint ksize, uint vsize, bool str) {
  if(!ksize) return NULL; // There needs to be a key
  int index = hash(k, ksize) % t->size;
  struct GENERIC_ITEM_* item = t->items + index;
  struct GENERIC_ITEM_* last = NULL;

  if(item->k != NULL) {
		switch (ksize) {
		case 4:
			do {
				if (*(unsigned int*) item->k == *(unsigned int*) k)
					return item->v = realloc(item->v, vsize);
				else last = item, item = item->next;
			} while (item != NULL);
			break;
		case 8:
			do {
				if (*(unsigned long*) item->k == *(unsigned long*) k)
					return item->v = realloc(item->v, vsize);
				else last = item, item = item->next;
			} while (item != NULL);
			break;
		default:
			if(str) do {
				if (strcmp(item->k, k) == 0)
					return item->v = realloc(item->v, vsize);
				else last = item, item = item->next;
			} while (item != NULL);
			else do {
				if (memcmp(item->k, k, ksize) == 0)
					return item->v = realloc(item->v, vsize);
				else last = item, item = item->next;
			} while (item != NULL);
		}
  } else t->filledbuckets ++;
  if(!item) item = malloc(sizeof(struct GENERIC_ITEM_));
  if(last) last->next = item;
  
  item->v = malloc(vsize);
  item->k = malloc(ksize);
  memcpy(item->k, k, ksize);
  item->next = NULL;
  pushkey(t, &item->k);
  return item->v;
}



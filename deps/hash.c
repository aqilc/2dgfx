/*
 * IDEAS:
  * I really hate that we have to store pointers to linked lists in every item, even when we require that 3/4s of the actual buffer be non-conflicts. It is wasting wasting 1/3rd of the memory for 3/4ts of the items, coming up to more than 1/4th of the whole hashtable being completely useless.
    What if we just remove the pointer to the next item(basically take out the linked list) and just put the item in conflict in the next row? We can still count conflicts, and although they would be more prevalent(since one conflict can now become another conflict), it would be much better for cache locality and memory usage.
   * This idea deviates from the idea of a hashtable completely, and I realize it's more like just using a hash function to kind of tell where something is lol. Also, it could be worse for perf but who knows, string comp is pretty fast and cache locality is important.
   [ ] Take off the `next` pointer
   [ ] Implement putting items beside the original bucket if there is a conflict
   [ ] Get items through checking every string key in conflict.
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

#include <hash.h>
#include <hashfunc.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// #include <stdio.h>

typedef unsigned int uint; // bc why

// Aligns to the next multiple of a, where a is a power of 2
static inline int align(int n, int a) { return (n + a - 1) & ~(a - 1); }
static inline void pushkey(struct GENERIC_TABLE_* t, void* ptr) {
  if(!t->keys) {
    t->keys = malloc(4 * sizeof(void*));
    t->keys[0] = ptr;
    t->n = 1;
    return;
  }
  int newsize = align(t->n + 1, 4);
  if(newsize != t->ksize)
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
  for(int i = 0; i < t->n; i++) {\
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
  for(int i = 0; i < t->n; i ++) {
    struct GENERIC_ITEM_* k = (struct GENERIC_ITEM_*) t->keys[i];
    if(k < t->items || k > t->items + t->size) continue; // Only select top level items, items that aren't separately allocated and linked to
    free(k->k); free(k->v);
    if(k->next) freeitem(k->next); // Much simpler to do it recursively here.
  }
  free(t->items);
  free(t->keys);
}

void htreset(struct GENERIC_TABLE_* t) {
  for(int i = 0; i < t->n; i ++) {
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



/*
Dump of assembler code for function htget:

push: Pushes a value onto the stack.
mov: Moves data from one location to another.
sub: Subtracts one value from another and stores the result.
test: Performs a bitwise AND operation on two values and sets flags based on the result.
jne: Jumps to a specified location if the zero flag is not set (i.e., if the result of the previous test or cmp instruction was not zero).
jmp: Unconditionally jumps to a specified location.
call: Calls a subroutine (i.e., jumps to a specified location and saves the return address on the stack).
div: Divides one value by another and stores the result.
add: Adds two values together and stores the result.
shl: Shifts a value to the left by a specified number of bits.
je: Jumps to a specified location if the zero flag is set (i.e., if the result of the previous test or cmp instruction was zero).
cmpb: Compares two values and sets flags based on the result.
pop: Pops a value off of the stack.
ret: Returns from a subroutine (i.e., pops the return address off of the stack and jumps to it).

  <+0>:     push   %rbp
  <+1>:     mov    %rsp,%rbp
  <+4>:     sub    $0x30,%rsp
  <+8>:     mov    %rcx,0x10(%rbp)
  <+12>:    mov    %rdx,0x18(%rbp)
  <+16>:    mov    %r8d,0x20(%rbp)
  <+20>:    mov    %r9d,%eax
  <+23>:    mov    %al,0x28(%rbp)
  <+26>:    mov    0x10(%rbp),%rax
  <+30>:    mov    0x4(%rax),%eax
  <+33>:    test   %eax,%eax
  <+35>:    jne    0x1400039c0 <htget+47>
  <+37>:    mov    $0x0,%eax
  <+42>:    jmp    0x140003a8d <htget+252>
  <+47>:    mov    0x20(%rbp),%edx
  <+50>:    mov    0x18(%rbp),%rax
  <+54>:    mov    %rax,%rcx
  <+57>:    call   0x140008d56 <hash>
  <+62>:    mov    0x10(%rbp),%rdx
  <+66>:    mov    0x4(%rdx),%edx
  <+69>:    mov    %edx,%ecx
  <+71>:    mov    $0x0,%edx
  <+76>:    div    %ecx
  <+78>:    mov    %edx,%eax
  <+80>:    mov    %eax,-0xc(%rbp)
  <+83>:    mov    0x10(%rbp),%rax
  <+87>:    mov    0x18(%rax),%rcx
  <+91>:    mov    -0xc(%rbp),%eax
  <+94>:    movslq %eax,%rdx
  <+97>:    mov    %rdx,%rax
  <+100>:   add    %rax,%rax
  <+103>:   add    %rdx,%rax
  <+106>:   shl    $0x3,%rax
  <+110>:   add    %rcx,%rax
  <+113>:   mov    %rax,-0x8(%rbp)
  <+117>:   mov    -0x8(%rbp),%rax
  <+121>:   mov    (%rax),%rax
  <+124>:   test   %rax,%rax
  <+127>:   je     0x140003a88 <htget+247>
  <+129>:   cmpb   $0x0,0x28(%rbp)
  <+133>:   je     0x140003a4e <htget+189>
  <+135>:   mov    -0x8(%rbp),%rax
  <+139>:   mov    (%rax),%rax
  <+142>:   mov    0x18(%rbp),%rdx
  <+146>:   mov    %rax,%rcx
  <+149>:   call   0x140010da0 <strcmp>
  <+154>:   test   %eax,%eax
  <+156>:   jne    0x140003a39 <htget+168>
  <+158>:   mov    -0x8(%rbp),%rax
  <+162>:   mov    0x8(%rax),%rax
  <+166>:   jmp    0x140003a8d <htget+252>
  <+168>:   mov    -0x8(%rbp),%rax
  <+172>:   mov    0x10(%rax),%rax
  <+176>:   mov    %rax,-0x8(%rbp)
  <+180>:   cmpq   $0x0,-0x8(%rbp)
  <+185>:   jne    0x140003a18 <htget+135>
  <+187>:   jmp    0x140003a88 <htget+247>
  <+189>:   mov    0x20(%rbp),%ecx
  <+192>:   mov    -0x8(%rbp),%rax
  <+196>:   mov    (%rax),%rax
  <+199>:   mov    0x18(%rbp),%rdx
  <+203>:   mov    %rcx,%r8
  <+206>:   mov    %rax,%rcx
  <+209>:   call   0x140010d58 <memcmp>
  <+214>:   test   %eax,%eax
  <+216>:   jne    0x140003a75 <htget+228>
  <+218>:   mov    -0x8(%rbp),%rax
  <+222>:   mov    0x8(%rax),%rax
  <+226>:   jmp    0x140003a8d <htget+252>
  <+228>:   mov    -0x8(%rbp),%rax
  <+232>:   mov    0x10(%rax),%rax
  <+236>:   mov    %rax,-0x8(%rbp)
  <+240>:   cmpq   $0x0,-0x8(%rbp)
  <+245>:   jne    0x140003a4e <htget+189>
  <+247>:   mov    $0x0,%eax
  <+252>:   add    $0x30,%rsp
  <+256>:   pop    %rbp
  <+257>:   ret
*/

// Gets an arbitrary type key from hash table
void* htget(struct GENERIC_TABLE_* t, void* k, uint ksize, bool str) {
  if(!t->size) return NULL; // If the table is empty, return NULL (no key found)
  int index = hash(k, ksize) % t->size;
  struct GENERIC_ITEM_* item = t->items + index;
  
  if(item->k != NULL) {
    if(str)
      do {
        if (strcmp(item->k, k) == 0)
          return item->v;
        else item = item->next;
      } while (item != NULL);
    else
      do {
        if (memcmp(item->k, k, ksize) == 0)
          return item->v;
        else item = item->next;
      } while (item != NULL);
  }

  return NULL;
}

// Sets/inserts an arbitrary type key in the given hash table
void* htset(struct GENERIC_TABLE_* t, void* k, uint ksize, size_t vsize, bool str) {
  if(!ksize) return NULL; // There needs to be a key
  int index = hash(k, ksize) % t->size;
  struct GENERIC_ITEM_* item = t->items + index;
  struct GENERIC_ITEM_* last = NULL;

  if(item->k != NULL) {
    if(str)
      do {
        if (strcmp(item->k, k) == 0)
          return item->v = realloc(item->v, vsize);
        else last = item, item = item->next;
      } while (item != NULL);
    else
      do {
        if (memcmp(item->k, k, ksize) == 0)
          return item->v = realloc(item->v, vsize);
        else last = item, item = item->next;
      } while (item != NULL);
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



#ifndef HASH_H
#define HASH_H
#include <stdbool.h>


#pragma GCC diagnostic ignored "-Wunused-value"

// Generic structs for sizing
struct GENERIC_ITEM_ { void* k; void* v; struct GENERIC_ITEM_* next; };
#define TABLEFIELDS unsigned int n; unsigned char size; bool stringkeys; unsigned int filledbuckets; unsigned int ksize;
struct GENERIC_TABLE_ {
  TABLEFIELDS

  void*** keys; // An ARRAY containing pointers to key POINTERS stored in the item, so requires 3 ptr things lol
  struct GENERIC_ITEM_* items;
};

#define HASH_STRIFY2_(x) #x
#define HASH_STRIFY_(x) HASH_STRIFY2_(x)

// #define HASH_CONCAT_RAW_(x, y, z) x##y##z
// #define HASH_CONCAT_(x, y, z) HASH_CONCAT_RAW_(x, y, z)

// Defines an item for the hashtable. Types for `k` get messed up when you pass in `char*` but we ignore it bc it barely matters.
// #define ITEMNAMEMANGLER HASH
// #define item(key, val) struct HASH_CONCAT(itemht_, __LINE__, ITEMNAMEMANGLER) { key* k; val* v; struct HASH_CONCAT(itemht_, __LINE__, ITEMNAMEMANGLER)* next; }
#define HASH_ITEM(key, val) struct { key* k; val* v; void* next; }

// Defines a hashtable type.
#define ht(key, val) struct { TABLEFIELDS\
  key** keys; HASH_ITEM(key, val)* items; }

// macros for easy hashtable method calling, main api:
#define hkeyt(htb) typeof(**(htb).keys)
#define hvalt(htb) typeof(*(htb).items->v)

// Resize if there are too many conflicts (n - filledbuckets). Aim for 3/4 filled buckets.
// What the current algorithm does, is when the number of filled buckets is less than 3/4s of the total items (conflicts exceeds 1/4), it resizes the table.

#define hget(htb, ...)   (hvalt(htb)*) htget((struct GENERIC_TABLE_*) &(htb), (hkeyt(htb)*) &__VA_ARGS__, sizeof(**(htb).keys), false)
#define hgetst(htb, ...) (hvalt(htb)*) htget((struct GENERIC_TABLE_*) &(htb), &(hkeyt(htb)) __VA_ARGS__, sizeof(**(htb).keys), false)
#define hgets(htb, key)  (hvalt(htb)*) htget((struct GENERIC_TABLE_*) &(htb), key, strlen(key) + 1, true)


#define hset(htb, ...) *(hvalt(htb)*) htset((struct GENERIC_TABLE_*) &(htb), (hkeyt(htb)*) &__VA_ARGS__, sizeof(**(htb).keys), sizeof(*(htb).items->v), false)
#define hsetst(htb, ...) *(hvalt(htb)*) htset((struct GENERIC_TABLE_*) &(htb), &(hkeyt(htb)) __VA_ARGS__, sizeof(**(htb).keys), sizeof(*(htb).items->v), false)
#define hsets(htb, key) *(hvalt(htb)*) htset((struct GENERIC_TABLE_*) &(htb), key, strlen(key) + 1, sizeof(*(htb).items->v), true)

#define hset_strcpy(str, htb, ...) strcpy((char*) htset((struct GENERIC_TABLE_*) &(htb), (hkeyt(htb)*) &__VA_ARGS__, sizeof(**(htb).keys), strlen(str), false), str)
#define hsetst_strcpy(str, htb, ...) strcpy((char*) htset((struct GENERIC_TABLE_*) &(htb), &(hkeyt(htb)) __VA_ARGS__, sizeof(**(htb).keys), strlen(str), false), str)
#define hsets_strcpy(str, htb, key) strcpy((char*) htset((struct GENERIC_TABLE_*) &(htb), key, strlen(key) + 1, strlen(str) + 1, true), str)

#define hfree(htb) htfree((struct GENERIC_TABLE_*) &(htb))
#define hreset(htb) htreset((struct GENERIC_TABLE_*) &(htb))

#define hlastv(htb) (((typeof((htb).items)) ((htb).keys[(htb).n - 1]))->v)
#define hgetn(htb, idx) (((typeof((htb).items)) ((htb).keys[idx]))->v)
#define hfirst(htb) ((typeof((htb).items)) ((htb).keys[0]))


#define hentry(htb) struct { hkeyt(htb) key; hvalt(htb) item; }
#define HTINITIALIZER_(htb) hentry(htb) htinitarr[]

// In a perfect world where _Generic was actually good. :(
/*
#define haddentries(htb, ...) do { HTINITIALIZER_(htb) = __VA_ARGS__;\
  htinit((struct GENERIC_TABLE_*) &(htb), sizeof(htinitarr));\
  for(unsigned int i = 0; i < sizeof(htinitarr) / htinitarr[0]; i++)\
    _Generic(**(htb).keys,\
      char*: _Generic(*(htb).items->v,\
        char*: (hsets_strcpy(htinitarr[i].item, htb, htinitarr[i].key)),\
        default: (hsets(htb, htinitarr[i].key) = htinitarr[i].item)\
      ),\
      default: Generic(*(htb).items->v,\
        char*: (hset_strcpy(htinitarr[i].item, htb, htinitarr[i].key)),\
        default: (hset(htb, htinitarr[i].key) = htinitarr[i].item)\
      )\
    );\
} while(0)
*/

#define hadd_entries(htb, ...) do { HTINITIALIZER_(htb) = __VA_ARGS__;\
  htinit((struct GENERIC_TABLE_*) &(htb), sizeof(htinitarr));\
  for(unsigned int i = 0; i < sizeof(htinitarr) / sizeof(htinitarr[0]); i++)\
    _Generic(**(htb).keys,\
      char*: (hsets(htb, htinitarr[i].key) = htinitarr[i].item),\
      default: (hset(htb, htinitarr[i].key) = htinitarr[i].item)\
    );\
} while(0)

#define hadd_entries_strcpy(htb, ...) do { HTINITIALIZER_(htb) = __VA_ARGS__;\
  htinit((struct GENERIC_TABLE_*) &(htb), sizeof(htinitarr));\
  for(unsigned int i = 0; i < sizeof(htinitarr) / sizeof(htinitarr[0]); i++)\
    _Generic(**(htb).keys,\
      char*: (hsets_strcpy(htinitarr[i].item, htb, htinitarr[i].key)),\
      default: (hset_strcpy(htinitarr[i].item, htb, htinitarr[i].key))\
    );\
} while(0)

#define hmerge_entries(htb, entries) do {\
  htinit((struct GENERIC_TABLE_*) &(htb), sizeof(entries));\
  for(unsigned int i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)\
    _Generic(**(htb).keys,\
      char*: (hsets(htb, entries[i].key) = entries[i].item),\
      default: (hset(htb, entries[i].key) = entries[i].item)\
    );\
} while(0)

#define hmerge_entries_strcpy(htb, entries) do {\
  htinit((struct GENERIC_TABLE_*) &(htb), sizeof(entries));\
  for(unsigned int i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)\
    _Generic(**(htb).keys,\
      char*: (hsets_strcpy(entries[i].item, htb, entries[i].key)),\
      default: (hset_strcpy(entries[i].item, htb, entries[i].key))\
    );\
} while(0)

// Raw Hashtable methods
void  htinit(struct GENERIC_TABLE_* t, unsigned int size);

bool  htresize(struct GENERIC_TABLE_* t, unsigned int size, unsigned int ksize);
bool  htsresize(struct GENERIC_TABLE_* t, unsigned int size);

void  htfree(struct GENERIC_TABLE_* t);
void  htreset(struct GENERIC_TABLE_* t);

void* htget(struct GENERIC_TABLE_* t, void* k, unsigned int ksize, bool str);

void* htset(struct GENERIC_TABLE_* t, void* k, unsigned int ksize, unsigned int vsize, bool str);


/*
struct customkey {
  int k;
  int keynum;
  char* idk;
};

ht(int, int) h;
ht(char*, int) h2;
ht(struct customkey, int) h3;

int main() {
  hgets(h2, "hello");
  hget(h3, { 1, 2, "hello" });

  hsets(h2, "hello") = 10;
  hset(h3, { 2, 3, "lol" }) = 5;
}
*/

#endif

#define HASH_H_IMPLEMENTATION
#ifdef HASH_H_IMPLEMENTATION

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


// Defined this file for a clear interface for the main hash function used in the project

#include <stdint.h>



#include <string.h>
#include <stdlib.h>


static inline uint32_t hash_h_hash_func(const char * data, uint32_t len);

#ifndef HASH_H_CUSTOM_HASHER
// http://www.azillionmonkeys.com/qed/hash.html
#include <stddef.h>
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

static inline uint32_t hash_h_hash_func(const char * data, uint32_t len) {
uint32_t hash = len, tmp;
int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits(data);
        tmp    = (get16bits(data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof(uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits(data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof(uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits(data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}
#endif

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
bool htresize(struct GENERIC_TABLE_* t, uint size, uint ksize) {
  if(!t->size) { htinit(t, size); return true; }
  struct GENERIC_ITEM_* new = calloc(sizeof(struct GENERIC_ITEM_), size);
  t->filledbuckets = 0;

  for(uint i = 0; i < t->n; i++) {
    uint ksize = ksize ?: strlen(*t->keys[i]) + 1;
    uint h = hash_h_hash_func(*t->keys[i], ksize);
    uint newind = h % size;

    struct GENERIC_ITEM_* item = (struct GENERIC_ITEM_*)t->keys[i];
    struct GENERIC_ITEM_* newitem = new + newind;

    /* If it's a top level item, we would have to copy it over or allocate it separately */
    if (item >= t->items && item < t->items + t->size) {
      if (newitem->k) {
        while(newitem->next) newitem = newitem->next;
        newitem->next = malloc(sizeof(struct GENERIC_ITEM_));
        newitem = newitem->next;
      } else t->filledbuckets ++; /* Need to know the new bucket count so we can resize accurately in the future. Biggest risk is causing subsequent resizes, but it would be very rare */
      newitem->k = item->k;
      newitem->v = item->v;
      newitem->next = NULL;
      t->keys[i] = &newitem->k;
      continue;
    }

    /* If it's not top level, it's separately allocated, so we need to free it in the instance we're setting to top level, or just set the item -> next to the new item */
    if (newitem->k) {
      while(newitem->next) newitem = newitem->next;
      newitem = (newitem->next = item);
      newitem->next = NULL;
    }
    else {
      newitem->k = item->k;
      newitem->v = item->v;
      /* newitem->next = NULL; // Already NULL*/
      t->filledbuckets ++;
      free(item);
    }
    t->keys[i] = &newitem->k;
  }

  free(t->items);
  t->items = new;
  t->size = size;
  return true;
}





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
  int index = hash_h_hash_func(k, ksize) % t->size;
  struct GENERIC_ITEM_* item = t->items + index;
  
  if(item->k != NULL) {
    switch (ksize) {
    case 4:
      do {
        if (*(uint*) item->k == *(uint*) k)
          return item->v;
        else item = item->next;
      } while (item != NULL);
      break;
    case 8:
      do {
        if (*(uint64_t*) item->k == *(uint64_t*) k)
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
  int index = hash_h_hash_func(k, ksize) % t->size;
  struct GENERIC_ITEM_* item = t->items + index;
  struct GENERIC_ITEM_* last = NULL;

  if(item->k != NULL) {
    switch (ksize) {
    case 4:
      do {
        if (*(uint*) item->k == *(uint*) k)
          return item->v = realloc(item->v, vsize);
        else last = item, item = item->next;
      } while (item != NULL);
      break;
    case 8:
      do {
        if (*(uint64_t*) item->k == *(uint64_t*) k)
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



#endif

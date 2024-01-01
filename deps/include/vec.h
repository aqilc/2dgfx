/*
 * vec.h v1.0.0 - Aqil Contractor @AqilC 2023
 * Licenced under Attribution-NonCommercial-ShareAlike 3.0
 *
 * This file includes all of the source for the Vector library macros and functions.
 * Compile by adding a file called `vec.c` with the following contents in your project:
 *     #define VEC_H_IMPLEMENTATION
 *     #include <vec.h>
 *
 * Options provided by defines:
 *     VEC_H_CALLOC:
 *       Name for user-provided calloc(size_t, size_t) function.
 *     VEC_H_REALLOC:
 *       Name for user-provided realloc(void*, size_t) function.
 *     VEC_H_FREE:
 *       Name for user-provided free(void*) function.
 *     VEC_H_OVERLOAD_ALLOCATORS:
 *       Define `void* vnew()` as you see fit, but after initial allocation, cast to struct vecdata_* and add 1 to pointer as done in current implementation.
 *       Define re-allocator function with the function signature: struct vecdata_* name(struct vecdata_* data, uint32_t size);
 *       Make sure to define VEC_H_REALLOC_FUNC with `name` after redefining.
 *     
 * WARNING: CURRENTLY NOT FULLY THREAD SAFE. Use with caution when using in thread safe code!
 */

#ifndef VEC_H
#define VEC_H


#if !defined VEC_H_FREE || !defined VEC_H_REALLOC || !defined VEC_H_CALLOC
	#include <stdlib.h>
#endif

#ifndef VEC_H_CALLOC
	#define VEC_H_CALLOC calloc
#endif

#ifndef VEC_H_REALLOC
	#define VEC_H_REALLOC realloc
#endif

#ifndef VEC_H_FREE
	#define VEC_H_FREE free
#endif

// #include <stdlib.h>
#include <stdint.h>
#include <string.h>

struct vecdata_ {
	uint32_t used;
	uint32_t cap;
	uint8_t data[];
};
#define _DATA(x) ((struct vecdata_*)(x) - 1)
#define vlen(x) (_DATA(x)->used / sizeof(*(x)))

// The normal push macro, pushes a value onto the obj
#define vpush(x, ...) (*((typeof(x)) vpush_((void**)&(x), sizeof(*(x)))) = (typeof(*x)) __VA_ARGS__)
// vpush(data, 5); expands to something like: *(int*)vpush_((void**) &data, 4) = (int) 5;

#define vpusharr(x, ...) (memcpy(vpush_((void**)&(x), sizeof((typeof(*(x))[]) __VA_ARGS__)), (typeof(*(x))[]) __VA_ARGS__, sizeof((typeof(*(x))[]) __VA_ARGS__)))

// Push n items onto the vector, so we can allocate more space at once
#define vpushn(x, n, y) /*_Generic(*(x), */vpushn_((void**) &(x), (n), sizeof(*(x)), &((typeof(*(x))) {y}))//)
// One for structs since that prev one didn't work for structs
#define vpushnst(x, n, y) vpushn_((void**) &(x), (n), sizeof(*(x)), &((typeof(*(x))) y))

// String push aliases so u don't have to &
#define vpushs(x, y) vpushs_((void**) &(x), (y))
#define vpushsf(x, ...) vpushsf_((void**) &(x), __VA_ARGS__)


// Push the entirety of a vec onto another
#define vpushv(x, y) (memcpy(vpush_((void**) &(x), _DATA(y)->used), y, _DATA(y)->used))

// Add values to the beginning of the vec
#define vunshift(x, ...) (*(typeof(x))vunshift_((void**)&(x), sizeof(*(x))) = (typeof(*x)) __VA_ARGS__)

// Simplifies vclear and vtostr calls
#define vclear(v) vclear_((void**)&(v))
#define vtostr(v) vtostr_((void**)&(v))

// Pops off the last element and returns it
#define vpop(x) vpop_((x), sizeof(*(x)))

// Removes data from the middle of the array
#define vremove(x, idx) vremove_((x), sizeof(*(x)), (idx))

// Pointer to the last element of the vector
#define vlast(x) ((x) + vlen(x) - 1)

// V String that has length info and is automatically push-able
typedef char* vstr;

// All you need to get started with this vector lib!
void* vnew();
// #define vnew() ((void*) ((struct vecdata_*) calloc(1, sizeof(struct vecdata_)) + 1))

// Returns a *new* concatenated vector, use `pushv` if you don't want a new vec :D
void* vcat(void* a, void* b);
void vfree(void* v);

// Returns a 1 if they're different, 0 if they're the same
char vcmp(void* a, void* b);

// Initialize a vector with a string straight away
char* strtov(char* s);
void  vempty(void* v);
char* vtostr_(void** v);
void  vremove_(void* v, uint32_t size, uint32_t pos);
void* vpush_(void** v, uint32_t size);
void  vpushs_(void** v, char* str);
void  vpushsf_(void** v, char* fmt, ...);
void  vpusharr_(void** v, uint32_t thingsize, void* thing);
void  vpushn_(void** v, uint32_t n, uint32_t size, void* thing);
void* vunshift_(void** v, uint32_t size);
void* vpop_(void* v, uint32_t size);
char* vfmt(char* str, ...);


#endif

#ifdef VEC_H_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>

char* fmtstr;

#ifndef VEC_H_OVERLOAD_ALLOCATORS
	// Callocs a vec with a cap of 16 so subsequent pushes don't immediately trigger reallocation.
	void* vnew() {
		struct vecdata_* v = VEC_H_CALLOC(1, sizeof(struct vecdata_) + 16 * sizeof(char));
		v->cap = 16;
		return v + 1;
	}
#endif

// Combines two vectors into a new vector
void* vcat(void* a, void* b) {
	struct vecdata_* v = VEC_H_CALLOC(1, ((struct vecdata_*) b)->used + ((struct vecdata_*) a)->used + sizeof(struct vecdata_));
	v->cap = v->used = ((struct vecdata_*) b)->used + ((struct vecdata_*) a)->used;
	v += 1;
	memcpy(v, a, _DATA(a)->used);
	memcpy((char*) v + _DATA(a)->used, b, _DATA(b)->used);
	return v;
}

char vcmp(void* a, void* b) {
	uint32_t len = ((struct vecdata_*) a)->used;
	if(len != ((struct vecdata_*) b)->used) return 1;
	uint32_t idx = 0;
	while (idx < len) if(((char*)a)[idx] != ((char*)b)[idx]) return 1; else idx ++;
	return 0;
}

char* strtov(char* s) {
	uint32_t len = strlen(s);
	struct vecdata_* v = VEC_H_CALLOC(1, len + sizeof(struct vecdata_));
	v->used = v->cap = len;
	v += 1;
	memcpy(v, s, len);
	return (char*) v;
}

char* vtostr_(void** v) {
	(*(char*)vpush_(v, 1)) = 0;
	_DATA(*v)->used --;
	return *v;
}

void vempty(void* v) { _DATA(v)->used = 0; }
void vfree(void* v) { VEC_H_FREE(_DATA(v)); }


// Reallocs more size for the array, hopefully without moves o.o
#ifndef VEC_H_OVERLOAD_ALLOCATORS
	#define VEC_H_REALLOC_FUNC alloc_
	static inline void* alloc_(struct vecdata_* data, uint32_t size) {
		data->used += size;
		if(data->cap < data->used) {
			data->cap = data->used + (data->used >> 2) + 16;
			return (struct vecdata_*)VEC_H_REALLOC(data, sizeof(struct vecdata_) + data->cap) + 1;
		}
		return data + 1;
	}
#elif !defined VEC_H_REALLOC_FUNC
	#error "Need to define re-allocator for the Vector library before implementation."
#endif


// Pushes more data onto the array, CAN CHANGE THE PTR U PASS INTO IT
static inline void* vpush__(void** v, uint32_t size) {
	struct vecdata_* data = _DATA(*v = VEC_H_REALLOC_FUNC(_DATA(*v), size));
	return data->data + data->used - size;
}
void* vpush_(void** v, uint32_t size) { return vpush__(v, size); }

// Allocates memory for a string and then pushes
void vpushs_(void** v, char* str) {
	uint32_t len = strlen(str);
	memcpy(vpush__(v, len + 1), str, len + 1);
	_DATA(*v)->used --;
}

// Gets length of formatted string to allocate from vector first, and then basically writes to the ptr returned by push
void vpushsf_(void** v, char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	va_list args2;
	va_start(args2, fmt);
	uint32_t len = vsnprintf(NULL, 0, fmt, args);
	vsnprintf(vpush__(v, len), len, fmt, args2);
	va_end(args);
	va_end(args2);
}

void vpushn_(void** v, uint32_t n, uint32_t size, void* thing) {
	char* place = vpush__(v, n * size);
	if(size == 1 || size == 4) memset(place, *((char*) thing), size);
	else for(int i = 0; i < n; i ++) memcpy(place + size * i, thing, size);
}

void* vpop_(void* v, uint32_t size) { _DATA(v)->used -= size; return _DATA(v)->data + _DATA(v)->used; }

// Adds an element at the start of the vector, ALSO CHANGES PTR
void* vunshift_(void** v, uint32_t size) {
	memmove((char*) (*v = alloc_(_DATA(*v), size)) + size, *v, _DATA(*v)->used);
	return *v;
}

// Deletes data from the middle of the array
void vremove_(void* v, uint32_t size, uint32_t pos) {
	memmove(_DATA(v) + pos, _DATA(v) + pos + size, _DATA(v)->used - pos - size);
	_DATA(v)->used -= size;
}

char* vfmt(char* str, ...) {
	if(!fmtstr) fmtstr = vnew();
	
	va_list args;
	va_start(args, str);
	va_list args2;
	va_start(args2, str);
	uint32_t len = vsnprintf(NULL, 0, str, args) + 1;
	if(len - _DATA(fmtstr)->used > 0)
		vpush__((void**) &fmtstr, len - _DATA(fmtstr)->used);
	vsnprintf(fmtstr, len, str, args2);
	va_end(args);
	va_end(args2);
	return fmtstr;
}
#endif

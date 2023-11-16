#ifndef VEC_H
#define VEC_H

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

#define vpusharr(x, ...) vpusharr_((void**)&(x), sizeof((typeof(*(x))[]) __VA_ARGS__), (typeof(*(x))[]) __VA_ARGS__)

// Push n items onto the vector, so we can allocate more space at once
#define vpushn(x, n, y) /*_Generic(*(x), */vpushn_((void**) &(x), (n), sizeof(*(x)), &((typeof(*(x))) {y}))//)
// One for structs since that prev one didn't work for structs
#define vpushnst(x, n, y) vpushn_((void**) &(x), (n), sizeof(*(x)), &((typeof(*(x))) y))

// String push aliases so u don't have to &
#define vpushs(x, y) vpushs_((void**) &(x), (y))
#define vpushsf(x, ...) vpushsf_((void**) &(x), __VA_ARGS__)


// Push the entirety of a vec onto another
#define vpushv(x, y) memcpy(vpush_((void**) &(x), _DATA(y)->used), y, _DATA(y)->used)

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

// push(x, ...) but basically for strings
// #define vpushs(x, y) memcpy((char*)(vpush_((void**)&(x), strlen(y))), (y), strlen(y))
// vpushs(data, "hello"); expands to something like: memcpy((char*)(vpush_((void**) &data, strlen("hello"))), "hello", strlen("hello"));

// The generic push macro, does not work :sob: pls find a way to make it work
// #define vpush(x, ...) _Generic((x), char*: memcpy((char*)(vpush_((void**)&(x), strlen(__VA_ARGS__))), __VA_ARGS__, strlen(__VA_ARGS__)),/*\*/
// 	default: (*(typeof(x))vpush_((void**)&(x), sizeof(*(x))) = (typeof(*x)) __VA_ARGS__))

// The variadic push macro, still in the works
// #define vpush(x, ...) (memcpy(vpush_((void**) &(x), sizeof((typeof(x)) { __VA_ARGS__ })), (typeof(x)) { __VA_ARGS__ }, sizeof((typeof(x)) { __VA_ARGS__ })))
// expands to: memcpy((int*)vpush_((void*) &data, 4), (((*data))[]) {5}, sizeof(((*data)[]) {5}));


// All you need to get started with this vector lib!
void* vnew();

// Returns a *new* concatenated vector, use `pushv` if you don't want a new vec :D
void* vcat(void* a, void* b);
void vfree(void* v);

// Returns a 1 if they're different, 0 if they're the same
char vcmp(void* a, void* b);

// Initialize a vector with a string straight away
char* strtov(char* s);
void vclear_(void** v);
void vempty(void* v);
char* vtostr_(void** v);
// void _alloc(struct vecdata_** data, u8 size);
void  vremove_(void* v, uint32_t size, uint32_t pos);
void* vpush_(void** v, uint32_t size);
void  vpushs_(void** v, char* str);
void  vpushsf_(void** v, char* fmt, ...);
void  vpusharr_(void** v, uint32_t thingsize, void* thing);
void  vpushn_(void** v, uint32_t n, uint32_t size, void* thing);
void* vunshift_(void** v, uint32_t size);
void* vpop_(void* v, uint32_t size);

#endif

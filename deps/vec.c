/*
 * vec.c v1.0.0 - Aqil Contractor @AqilCont 2023
 * Licenced under Attribution-NonCommercial-ShareAlike 3.0
 *
 * This file includes all of the source for the Vector library macros and functions.
 * Compile this separately into a .o, .obj, .a, .dll or .lib and link into your project to use it appropriately.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "vec.h"

// Just callocs a vec lol
void* vnew() { return (struct vecdata_*)calloc(1, sizeof(struct vecdata_)) + 1; }

// Combines two vectors into a new vector (USE THIS FOR STRING VECS INSTEAD OF _PUSHS PLS I BEG)
void* vcat(void* a, void* b) {
	void* v = vnew();
	vpush_(&v, ((struct vecdata_*) b)->used + ((struct vecdata_*) a)->used);
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
	char* v = vnew();
	vpushs_((void**) &v, s);
	return v;
}

char* vtostr_(void** v) {
	(*(char*)vpush_(v, 1)) = 0;
	_DATA(*v)->used --;
	return *v;
}

void vclear_(void** v) {
	struct vecdata_* data = realloc((struct vecdata_*) *v - 1, sizeof(struct vecdata_));
	*v = data + 1;
	data->cap = 0;
	data->used = 0;
}

void vempty(void* v) {
	_DATA(v)->used = 0;
}

void vfree(void* v) { free(_DATA(v)); }


// Reallocs more size for the array, hopefully without moves o.o
void* alloc_(struct vecdata_* data, uint32_t size) {
	data->used += size;
	if(data->cap < data->used) {
		data->cap += data->used - data->cap;
		return (struct vecdata_*)realloc(data, sizeof(struct vecdata_) + data->cap) + 1;
	}
	return data + 1;
}

// Pushes more data onto the array, CAN CHANGE THE PTR U PASS INTO IT
void* vpush_(void** v, uint32_t size) {
	struct vecdata_* data = _DATA(*v = alloc_(_DATA(*v), size));
	return data->data + data->used - size;
}

// Allocates memory for a string and then pushes
void vpushs_(void** v, char* str) {
	uint32_t len = strlen(str);
	memcpy(vpush_(v, len + 1), str, len + 1);
	_DATA(*v)->used --;
}

// Gets length of formatted string to allocate from vector first, and then basically writes to the ptr returned by push
void vpushsf_(void** v, char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	va_list args2;
	va_start(args2, fmt);
	uint32_t len = vsnprintf(NULL, 0, fmt, args);
	vsnprintf(vpush_(v, len), len, fmt, args2);
	va_end(args);
	va_end(args2);
}

void vpushn_(void** v, uint32_t n, uint32_t size, void* thing) {
	char* place = vpush_(v, n * size);
	if(size == 1) memset(place, *((char*) thing), size);
	else for(int i = 0; i < n; i ++) memcpy(place + size * i, thing, size);
}

void vpusharr_(void** v, uint32_t thingsize, void* thing) { memcpy(vpush_(v, thingsize), thing, thingsize); }

void* vpop_(void* v, uint32_t size) { _DATA(v)->used -= size; return _DATA(v)->data + _DATA(v)->used; }

// Adds an element at the start of the vector, ALSO CHANGES PTR
void* vunshift_(void** v, uint32_t size) {
	memmove((char*) (*v = alloc_(_DATA(*v), size)) + size, *v, ((struct vecdata_*) *v)->used);
	return *v;
}

// Deletes data from the middle of the array
void vremove_(void* v, uint32_t size, uint32_t pos) {
	memmove(_DATA(v) + pos, _DATA(v) + pos + size, _DATA(v)->used - pos - size);
	_DATA(v)->used -= size;
}


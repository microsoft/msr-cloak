#ifndef CACHEUTILS_H
#define CACHEUTILS_H

#include <intrin.h>

#ifndef HIDEMINMAX
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#endif

uint64_t rdtsc() {
	_mm_mfence();
	auto t = __rdtsc();
	_mm_mfence();
	return t;
}

void maccess(void* p) {
  *(volatile size_t*)p;
}

void flush(void* p) {
	_mm_clflush(p);
}

#endif

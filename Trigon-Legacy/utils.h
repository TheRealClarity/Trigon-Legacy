// utils.h
// Trigon-Legacy, 2025

#ifndef utils_h
#define utils_h

#include "common.h"

void hexdump(const void* data, size_t size);
void* reverse_memmem (const void *haystack, size_t haystack_len, const void *needle, size_t needle_len);
#endif /* utils_h */

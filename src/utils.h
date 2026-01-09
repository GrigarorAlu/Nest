// utils.h

#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <string.h>

void *addElement(void *vector, size_t length, const void *data, size_t pos, size_t size);

void *removeElement(void *vector, size_t length, size_t pos, size_t size);

char *strdup(const char *src);

char *strndup(const char *src, size_t size);

void *memdup(const void *src, size_t size);

#endif

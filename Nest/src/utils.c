// utils.c

#include "utils.h"

void *addElement(void *vector, size_t length, const void *data, size_t pos, size_t size)
{
    vector = realloc(vector, (length + 1) * size);
    if (pos < length)
        memmove((char *)vector + (pos + 1) * size, (char *)vector + pos * size, (length - pos) * size);
    if (data)
        memcpy((char *)vector + pos * size, data, size);
    else
        memset((char *)vector + pos * size, 0, size);
    return vector;
}

void *removeElement(void *vector, size_t length, size_t pos, size_t size)
{
    if (pos < --length)
        memmove((char *)vector + pos * size, (char *)vector + (pos + 1) * size, length - pos);
    return realloc(vector, (length * size));
}

char *strdup(const char *src)
{
    return strcpy(malloc(strlen(src) + 1), src);
}

char *strndup(const char *src, size_t size)
{
    char *str = malloc(size + 1);
    str[size] = 0;
    return memcpy(str, src, size);
}

void *memdup(const void *src, size_t size)
{
    return memcpy(malloc(size), src, size);
}

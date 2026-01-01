// protocol.c

#include "protocol.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

size_t serializeLen(const char *format, ...)
{
    int count = 0;
    size_t len = 0;
    va_list args;
    va_start(args, format);
    
    while (*format)
    {
        switch (*format++)
        {
        case 'c':
            len += 1;
            break;
        case 'h':
            len += 2;
            break;
        case 'i':
            len += 4;
            break;
        case 'l':
            len += 8;
            break;
        case 't':
            len += datetimeLen;
            break;
        case 's':
            count++;
            len += 2;
            break;
        default:
            va_end(args);
            return 0;
        }
    }
    
    for (int i = 0; i < count; i++)
    {
        const char *s = va_arg(args, const char *);
        if (s)
            len += strlen(s);
    }
    
    va_end(args);
    return len;
}

char *serializeInto(char *data, const char *format, ...)
{
    int count = strlen(format);
    va_list args;
    va_start(args, format);
    
    while (*format)
    {
        switch (*format++)
        {
        case 'c':
            int c = va_arg(args, int);
            memcpy(data, &c, 1);
            data += 1;
            break;
        case 'h':
            int h = va_arg(args, int);
            memcpy(data, &h, 2);
            data += 2;
            break;
        case 'i':
            int i = va_arg(args, int);
            memcpy(data, &i, 4);
            data += 4;
            break;
        case 'l':
            size_t l = va_arg(args, size_t);
            memcpy(data, &l, 8);
            data += 8;
            break;
        case 't':
            const char *t = va_arg(args, const char *);
            memcpy(data, t, datetimeLen);
            data += datetimeLen;
            break;
        case 's':
            const char *s = va_arg(args, const char *);
            if (s)
            {
                short len = strlen(s);
                memcpy(data, &len, 2);
                memcpy(data + 2, s, len);
                data += len;
            }
            else
                memset(data, 0, 2);
            data += 2;
            break;
        default:
            va_end(args);
            return 0;
        }
    }
    
    va_end(args);
    return data;
}

#include <stdio.h>

char *serializeNew(const char *format, ...)
{
    int count = strlen(format);
    size_t len = 0;
    va_list args;
    va_start(args, format);
    
    for (int i = 0; i < count; i++)
    {
        switch (format[i])
        {
        case 'c':
            va_arg(args, int);
            len += 1;
            break;
        case 'h':
            va_arg(args, int);
            len += 2;
            break;
        case 'i':
            va_arg(args, int);
            len += 4;
            break;
        case 'l':
            va_arg(args, size_t);
            len += 8;
            break;
        case 't':
            va_arg(args, const char *);
            len += datetimeLen;
            break;
        case 's':
            const char *s = va_arg(args, const char *);
            if (s)
                len += strlen(s);
            len += 2;
            break;
        default:
            va_end(args);
            return 0;
        }
    }
    
    va_end(args);
    char *data = malloc(len);
    va_start(args, format);
    
    for (char *p = data; *format;)
    {
        switch (*format++)
        {
        case 'c':
            char c = va_arg(args, int);
            memcpy(p, &c, 1);
            p += 1;
            break;
        case 'h':
            short h = va_arg(args, int);
            memcpy(p, &h, 2);
            p += 2;
            break;
        case 'i':
            int i = va_arg(args, int);
            memcpy(p, &i, 4);
            p += 4;
            break;
        case 'l':
            size_t l = va_arg(args, size_t);
            memcpy(p, &l, 4);
            p += 4;
            break;
        case 't':
            const char *t = va_arg(args, const char *);
            memcpy(p, t, datetimeLen);
            p += datetimeLen;
            break;
        case 's':
            const char *s = va_arg(args, const char *);
            if (s)
            {
                short len = strlen(s);
                memcpy(p, &len, 2);
                memcpy(p + 2, s, len);
                p += len;
            }
            else
                memset(p, 0, 2);
            p += 2;
            break;
        default:
            va_end(args);
            return 0;
        }
    }
    
    va_end(args);
    return data;
}

char *deserialize(char *data, size_t maxLen, const char *format, ...)
{
    int count = strlen(format);
    va_list args;
    va_start(args, format);
    
    for (int i = 0, pos = 0; i < count; i++)
    {
        switch (format[i])
        {
        case 'c':
            pos += 1;
            if (pos > maxLen)
                goto err;
            memcpy(va_arg(args, char *), data, 1);
            data += 1;
            break;
        case 'h':
            pos += 2;
            if (pos > maxLen)
                goto err;
            memcpy(va_arg(args, short *), data, 2);
            data += 2;
            break;
        case 'i':
            pos += 4;
            if (pos > maxLen)
                goto err;
            memcpy(va_arg(args, int *), data, 4);
            data += 4;
            break;
        case 'l':
            pos += 8;
            if (pos > maxLen)
                goto err;
            memcpy(va_arg(args, size_t *), data, 8);
            data += 8;
            break;
        case 't':
            pos += datetimeLen;
            if (pos > maxLen)
                goto err;
            if (memchr(data, 0, datetimeLen))
                goto err;
            char **t = va_arg(args, char **);
            *t = malloc(datetimeLen + 1);
            memcpy(*t, data, datetimeLen);
            (*t)[datetimeLen] = 0;
            data += datetimeLen + 1;
            break;
        case 's':
            pos += 2;
            if (pos > maxLen)
                goto err;
            char **s = va_arg(args, char **);
            short len;
            memcpy(&len, data, 2);
            if (len)
            {
                pos += len;
                if (pos > maxLen)
                    goto err;
                if (memchr(data + 2, 0, len))
                    goto err;
                *s = malloc(len + 1);
                (*s)[len] = 0;
                memcpy(*s, data + 2, len);
            }
            else
            {
                *s = malloc(1);
                **s = 0;
            }
            data += len + 2;
            break;
        default:
            goto err;
        }
    }
    
    va_end(args);
    return data;

err:
    va_end(args);
    return 0;
}

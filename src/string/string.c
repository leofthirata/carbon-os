#include "string.h"

int strnlen(const char *ptr, int max)
{
    int i = 0;
    for (i = 0; i < max; i++)
    {
        if (ptr[i] == 0)
            break;
    }

    return i;
}

int strlen(const char *ptr)
{
    int count = 0;

    while (*ptr != 0)
    {
        count++;
        ptr++;
    }

    return count;
}

bool is_digit(char c)
{
    return c >= 48 && c <= 57;
}

int char_to_digit(char c)
{
    return c - 48;
}

char *strcpy(char *dest, const char *src)
{
    char *res = dest;

    while (*src != 0)
    {
        *dest = *src;
        dest++;
        src++;
    }

    *dest = 0;

    return res;
}
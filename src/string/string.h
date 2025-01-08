#ifndef STRING_H
#define STRING_H

#include <stdbool.h>

bool is_digit(char c);
int char_to_digit(char c);
int strlen(const char *ptr);
int strnlen(const char *ptr, int max);

#endif
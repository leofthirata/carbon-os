#ifndef STRING_H
#define STRING_H

#include <stdbool.h>

bool is_digit(char c);
int char_to_digit(char c);
int strlen(const char *ptr);
int strnlen(const char *ptr, int max);
char *strcpy(char *dest, const char *src);
int strncmp(const char *str1, const char *str2, int n);
int istrncmp(const char *s1, const char *s2, int n);
int strnlen_terminator(const char *str, int max, char terminator);
char to_lower(char s1);

#endif
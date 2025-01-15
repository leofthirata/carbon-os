#ifndef KERNEL_H
#define KERNEL_H

#define VGA_WIDTH 80 // default
#define VGA_HEIGHT 20 // default

#define CARBONOS_PATH_MAX_SIZE 108 

#define COLOUR_WHITE 15

void kernel_main();
void print(const char* str);
void panic(const char *msg);

// cast to void *
#define ERROR(value) (void*)(value)
// cast to int
#define ERROR_I(value) (int)(value)
// cast to int and check if below zero
#define ISERR(value) ((int)value < 0)

#endif
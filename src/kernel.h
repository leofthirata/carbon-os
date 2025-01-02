#ifndef KERNEL_H
#define KERNEL_H

#define VGA_WIDTH 80 // default
#define VGA_HEIGHT 20 // default

#define COLOUR_WHITE 15

void kernel_main();
void print(const char* str);

#endif
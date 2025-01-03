#include "kernel.h"
#include "idt/idt.h"
#include "io/io.h"
#include "memory/heap/kheap.h"

#include <stdint.h> // for uint16_t
#include <stddef.h> // size_t

uint16_t *video_mem = 0;
uint16_t terminal_row = 0;
uint16_t terminal_col = 0;

extern void problem();
extern void problem2();

uint16_t terminal_print_char(char c, char colour)
{
    return (colour << 8) | c;
}

void terminal_putchar(int x, int y, char c, char colour)
{
    video_mem[(y * VGA_WIDTH) + x] = terminal_print_char(c, colour);
}

void terminal_writechar(char c, char colour)
{
    if (c == '\n')
    {
        terminal_row++;
        terminal_col = 0;
        return;
    }

    terminal_putchar(terminal_col, terminal_row, c, colour);
    terminal_col++;

    if (terminal_col >= VGA_WIDTH) // reset when it reaches end of screen
    {
        terminal_col = 0;
        terminal_row++;
    }
}

void terminal_init()
{
    video_mem = (uint16_t *)(0xB8000); // display address
    
    for (int y = 0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            // video_mem[(y * VGA_WIDTH) + x] = terminal_print_char(' ', 0);
            terminal_putchar(x, y, ' ', 0);
        }
    }
}

size_t strlen(const char* str)
{
    size_t len = 0;
    while(str[len])
    {
        len++;
    }

    return len;
}

void print(const char* str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        terminal_writechar(str[i], COLOUR_WHITE);
    }
}

void kernel_main()
{
    // char *video_mem = (char *)(0xB8000); // display address
    // video_mem[0] = 'A';
    // video_mem[1] = 2;
    // video_mem[2] = 'C';
    // video_mem[3] = 2;

    // uint16_t *video_mem = (uint16_t *)(0xB8000); // display address
    
    terminal_init();
    // video_mem[0] = 0x0341; // endian ascii A -> 0x4103
    // video_mem[1] = terminal_print_char('X', COLOUR_WHITE); // endian ascii A -> 0x4103

    // terminal_writechar('A', COLOUR_WHITE);
    // terminal_writechar('B', COLOUR_WHITE);
    print("CarbonOS!\nHello");

    kheap_init();

    // Interrupt Descriptor Table init
    idt_init();

    enable_it(); // enable interrupts
    
    // outb(0x60, 0xFF);
    // problem();

    // heap management testing
    void *ptr = kmalloc(50);
    void *ptr2 = kmalloc(5000);
    void *ptr3 = kmalloc(5600);
    kfree(ptr);
    void *ptr4 = kmalloc(50);
    if (ptr || ptr2 || ptr3 || ptr4)
    {

    }
}
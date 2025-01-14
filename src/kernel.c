#include "kernel.h"
#include "idt/idt.h"
#include "io/io.h"
#include "memory/heap/kheap.h"
#include "memory/paging/paging.h"
#include "disk/disk.h"
#include "disk/streamer.h"
#include "fs/pparser.h"
#include "string/string.h"
#include "fs/fat/fat16.h"

#include <stdint.h> // for uint16_t
#include <stddef.h> // size_t

uint16_t *video_mem = 0;
uint16_t terminal_row = 0;
uint16_t terminal_col = 0;
static struct paging_4gb_chunk *kernel_chunk = 0;

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
    // https://wiki.osdev.org/Printing_To_Screen
    // 0xB8000 for protected mode

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
    print("CarbonOS!\n");

    // Heap init
    kheap_init();

    // File system init
    fs_init();

    // Search and initialize the disks
    disk_search_and_init();

    // Interrupt Descriptor Table init
    idt_init();

    // page dir setup
    kernel_chunk = paging_new_4gb(PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);

    // switch to kernel paging chunk
    paging_switch(paging_4gb_chunk_get_dir(kernel_chunk));

    // paging testing
    // ptr is our physical addr and 0x1000 is the desired virtual address
    // we set 0x1000 virtual addr to point to whichever physical addr ptr is pointing to
    // char *ptr = kzalloc(4096);
    // paging_set(paging_4gb_chunk_get_dir(kernel_chunk), (void *)0x1000, (uint32_t) ptr | PAGING_ACCESS_FROM_ALL | PAGING_IS_PRESENT | PAGING_IS_WRITEABLE);

    paging_enable();

    // paging testing
    // ptr2 points to 0x1000 which points to physical addr ptr
    // char *ptr2 = (char *)0x1000;
    // ptr[0] = 'A';
    // ptr[1] = 'B';
    // print(ptr2);
    // print(ptr);

    // disk read testing
    // char buf[512];

    // disk_read_sector(0, 1, buf); // place breakpoint here and, in gdb, print (unsigned char)(buf[0]) to see first byte read
    // can use bless ./bin/os.bin to read the sector as well

    enable_it(); // enable interrupts

    // path parser testing
    // struct path_root *root = pparser_parse("0:/bin/shell.exe", NULL);
    // if (root)
    // {

    // }

    // disk streamer testing
    // struct disk_stream *stream = disk_streamer_new(0);
    // disk_streamer_seek(stream, 0x201);
    // unsigned char c = 0;
    // disk_streamer_read(stream, &c, 1);

    int fd = fopen("0:/hello.txt", "r");
    if (fd)
    {
        print("FOPEN hello.txt\n");
        char buf[9];
        buf[8] = 0x00;
        fread(buf, 9, 1, fd);
        print(buf);
    }

    // strcpy method testing
    // char buf[20];
    // strcpy(buf, "teste");

    while (1)
    {

    }

    // outb(0x60, 0xFF);
    // problem();

    // heap management testing
    // void *ptr = kmalloc(50);
    // void *ptr2 = kmalloc(5000);
    // void *ptr3 = kmalloc(5600);
    // kfree(ptr);
    // void *ptr4 = kmalloc(50);
    // if (ptr || ptr2 || ptr3 || ptr4)
    // {

    // }
}
#include <io/io.h>

// https://wiki.osdev.org/ATA_read/write_sectors
// https://wiki.osdev.org/ATA_Command_Matrix

// read in LBA mode
int disk_read_sector(int lba, int total, void *buf)
{ 
    // 0x1F6 port to send drive and bit 24-27 of lba
    // 0xE0 Set bit 6 in al for LBA mode
    outb(0x1F6, (lba >> 24) | 0xE0);
    // 0x1F2 sector count port
    outb(0x1F2, total);
    // 0x1F3 port to send bit 0-7 of lba
    outb(0x1F3, (unsigned char)(lba & 0xFF));
    // 0x1F4 port to send bit 8-15 of lba
    outb(0x1F4, (unsigned char)(lba >> 8));
    // 0x1F5 port to send bit 16-23 of lba
    outb(0x1F5, (unsigned char)(lba >> 16));
    // 0x1F7 command port
    outb(0x1F7, 0x20);

    unsigned short *ptr = (unsigned short *)buf;
    for (int i = 0; i < total; i++)
    {
        // hardware send data to buffer port 0x1F7 and we read a byte
        char c = insb(0x1F7);
        while (!(c & 0x08))
        {
            c = insb(0x1F7);
        }
    }

    for (int i = 0; i < 256; i++)
    {
        // read word
        *ptr = insw(0x1F0);
        ptr++;
    }

    return 0;
}
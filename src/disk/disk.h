#ifndef DISK_H
#define DISK_H

#include "fs/file.h"

// real hard disk representation
#define CARBONOS_DISK_TYPE_REAL 0

typedef unsigned int CARBONOS_DISK_TYPE;

struct disk
{
    CARBONOS_DISK_TYPE type;
    int sector_size;

    struct file_system *file_system;
};

void disk_search_and_init();
struct disk *disk_get(int index);
int disk_read_block(struct disk *idisk, unsigned int lba, int total, void *buf);

#endif
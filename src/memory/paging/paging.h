#ifndef PAGING_H
#define PAGING_H

// https://wiki.osdev.org/Paging

#include <stdint.h>
#include <stddef.h>

#define PAGING_CACHE_DISABLED   0b00010000
#define PAGING_WRITE_THOURGH    0b00001000
#define PAGING_ACCESS_FROM_ALL  0b00000100
#define PAGING_IS_WRITEABLE     0b00000010
#define PAGING_IS_PRESENT       0b00000001

#define PAGING_DIR_ENTRIES              1024
#define PAGING_TABLE_ENTRIES            1024
#define PAGING_PAGE_SIZE                4096

struct paging_4gb_chunk
{
    uint32_t *dir_entry;
};

struct paging_4gb_chunk *paging_new_4gb(uint8_t flags);
void paging_switch(uint32_t *dir);
uint32_t *paging_4gb_chunk_get_dir(struct paging_4gb_chunk *chunk);
void paging_enable();

#endif
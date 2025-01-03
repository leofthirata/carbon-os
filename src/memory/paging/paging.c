#include "paging.h"
#include "memory/heap/kheap.h"

void paging_load_dir(uint32_t *dir);

static uint32_t *current_dir = 0;

struct paging_4gb_chunk *paging_new_4gb(uint8_t flags)
{
    // init page directory entries
    uint32_t *dir = kzalloc(sizeof(uint32_t) * PAGING_TABLE_ENTRIES);
    
    int offset = 0;

    for (int i = 0; i < PAGING_TABLE_ENTRIES; i++)
    {
        // init page table entries
        uint32_t *entry = kzalloc(sizeof(uint32_t) * PAGING_TABLE_ENTRIES);

        for (int j = 0; j < PAGING_TABLE_ENTRIES; j++)
        {
            // set page addresses
            entry[j] = (offset + (j * PAGING_PAGE_SIZE)) | flags;
        }
        // offset for every table
        offset += (PAGING_TABLE_ENTRIES * PAGING_PAGE_SIZE);
        dir[i] = (uint32_t) entry | flags | PAGING_IS_WRITEABLE;        
    }

    struct paging_4gb_chunk *chunk_4gb = kzalloc(sizeof(struct paging_4gb_chunk));
    chunk_4gb->dir_entry = dir;

    return chunk_4gb;
}

void paging_switch(uint32_t *dir)
{
    paging_load_dir(dir);
    current_dir = dir;
}

uint32_t *paging_4gb_chunk_get_dir(struct paging_4gb_chunk *chunk)
{
    return chunk->dir_entry;
}
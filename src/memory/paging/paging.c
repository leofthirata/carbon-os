#include "paging.h"
#include "memory/heap/kheap.h"
#include "status.h"

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

// check if addr is multiple of 4096
bool paging_is_aligned(void *addr)
{
    return (uint32_t)addr % PAGING_PAGE_SIZE == 0;
}

// which page dir is being used and which page table is being used
int paging_get_index(void *virtual_addr, uint32_t *dir_index, uint32_t *table_index)
{
    int res = 0;

    if (!paging_is_aligned(virtual_addr))
    {
        res = -EINVARG;
        goto out;
    }

    // get which directory the page is indexed to
    *dir_index = ((uint32_t)virtual_addr / (PAGING_TABLE_ENTRIES * PAGING_PAGE_SIZE));

    // get which table the page is indexed to
    *table_index = ((uint32_t)virtual_addr % (PAGING_TABLE_ENTRIES * PAGING_PAGE_SIZE) / PAGING_PAGE_SIZE);

out:
    return res;
}

// virtual addr points to physical addr val
// virtual addr is translated to a directory index and then to a table index which is a page, and the page points to physical addr
int paging_set(uint32_t *dir, void *virtual_addr, uint32_t val)
{
    if (!paging_is_aligned(virtual_addr))
    {
        return -EINVARG;
    }

    // table
    uint32_t dir_idx = 0;
    // page
    uint32_t table_idx = 0;

    int res = paging_get_index(virtual_addr, &dir_idx, &table_idx);
    if (res < 0)
    {
        return res;
    }

    // table
    uint32_t entry = dir[dir_idx];

    // 0xFFFFF000 for the 20 bits of page table 4kb aligned addr
    // https://wiki.osdev.org/Paging
    // the rest of the bits arent used in 4096 alignment, so we ignore
    uint32_t *table = (uint32_t *)(entry & 0xFFFFF000);
    table[table_idx] = val;

    return res;
}
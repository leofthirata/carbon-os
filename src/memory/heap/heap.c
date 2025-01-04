#include "heap.h"
#include "status.h"
#include "memory/memory.h"

#include <stdbool.h> 

// check if has enough heap for desired amount of heap to avoid overflow
static int heap_validate_table(void *ptr, void *end, struct heap_table *table)
{
    int res = 0;

    size_t table_size = (size_t)(end - ptr);
    size_t total_blocks = table_size / CARBONOS_HEAP_BLOCK_SIZE;
    if (table->total != total_blocks)
    {
        res = -EINVARG;
        goto out;
    }

out:
    return res;
}

// check if heap start and end address are aligned with 4096 (multiple of 4096)
static bool heap_validate_alignment(void *ptr)
{
    return ((unsigned int) ptr % CARBONOS_HEAP_BLOCK_SIZE == 0);
}

int heap_create(struct heap *heap, void *start, void *end, struct heap_table *table)
{
    int res = 0;

    if (!heap_validate_alignment(start) || !heap_validate_alignment(end))
    {
        res = -EINVARG;
        goto out;
    }

    memset(heap, 0, sizeof(struct heap));
    heap->saddr = start;
    heap->table = table;

    res = heap_validate_table(start, end, table);
    if (res < 0)
    {
        goto out;
    }

    size_t table_size = sizeof(HEAP_BLOCK_TABLE_ENTRY) * table->total; // calc table size
    memset(table->entries, HEAP_BLOCK_TABLE_ENTRY_FREE, table_size); // set all blocks as free

out:
    return res;
}

static uint32_t heap_align_value_upper(uint32_t val)
{
    if ((val % CARBONOS_HEAP_BLOCK_SIZE) == 0)
    {
        return val;
    }

    // basically val % 4096 + 1
    val = (val - (val % CARBONOS_HEAP_BLOCK_SIZE));
    val += CARBONOS_HEAP_BLOCK_SIZE;

    return val;
}

static int heap_get_entry_type(HEAP_BLOCK_TABLE_ENTRY entry)
{
    return entry & 0x0F;
}

// find first possible block that has enough available blocks in series
int heap_get_start_block(struct heap *heap, uint32_t blocks)
{
    struct heap_table *table = heap->table;
    int current_block = 0;
    int start_block = -1;

    for (size_t i = 0; i < table->total; i++)
    {
        if (heap_get_entry_type(table->entries[i]) != HEAP_BLOCK_TABLE_ENTRY_FREE)
        {
            // restart if no enough blocks available in series 
            current_block = 0;
            start_block = -1;
            continue;
        }

        if (start_block == -1) // found first free block
        {
            start_block = i;
        }

        current_block++;

        if (current_block == blocks) // found enough needed blocks
        {
            break;
        }
    }

    if (start_block == -1)
    {
        return -ENOMEM;
    }

    return start_block;
}

void heap_mark_blocks_free(struct heap *heap, uint32_t start_block)
{
    // loops until last block from the entry
    for (int i = start_block; i < (int)heap->table->total; i++)
    {
        // if last block
        if (!(heap->table->entries[i] & HEAP_BLOCK_HAS_NEXT))
        {
            // set last entry as free
            heap->table->entries[i] = HEAP_BLOCK_TABLE_ENTRY_FREE;
            break;
        }
    
        // set entry as free
        heap->table->entries[i] = HEAP_BLOCK_TABLE_ENTRY_FREE;
    }
}

// converts address to block index
// get starting block index
uint32_t heap_addr_to_block(struct heap *heap, void *addr)
{
    return ((uint32_t)(addr - heap->saddr)) / CARBONOS_HEAP_BLOCK_SIZE;
}

// converts block index to its address
void *heap_block_to_addr(struct heap *heap, uint32_t block)
{
    return heap->saddr + (block * CARBONOS_HEAP_BLOCK_SIZE);
}

void heap_mark_blocks_taken(struct heap *heap, int start_block, int blocks)
{
    // starts at 0
    int end_block = (start_block + blocks) - 1;

    for (int i = start_block; i <= end_block; i++)
    {
        // set entry as taken
        heap->table->entries[i] = HEAP_BLOCK_TABLE_ENTRY_TAKEN;
        if (i == start_block)
        {
            // if first entry, set is first flag
            heap->table->entries[i] |= HEAP_BLOCK_IS_FIRST;
        }
        
        // if there are more than 1 blocks, set all entries with HASN but the last one 
        if (blocks > 1 && i != end_block - 1)
        {
            heap->table->entries[i] |= HEAP_BLOCK_HAS_NEXT;
        }
    }
}

// check for overflow and then set blocks as taken
void *heap_malloc_blocks(struct heap *heap, uint32_t blocks)
{
    void *addr = 0;
    
    int start_block = heap_get_start_block(heap, blocks);
    if (start_block < 0)
    {
        goto out;
    }

    addr = heap_block_to_addr(heap, start_block);

    heap_mark_blocks_taken(heap, start_block, blocks);

out:
    return addr;
}

void *heap_malloc(struct heap *heap, size_t size)
{
    // gives new proper size rounding to upper
    // defined block size is 4096
    // if desired heap size is 4097, then block count is rounded to 2 and actual block size is 4096 * 2
    size_t aligned_size = heap_align_value_upper(size);
    uint32_t needed_blocks = aligned_size / CARBONOS_HEAP_BLOCK_SIZE; // calc how many blocks are needed

    return heap_malloc_blocks(heap, needed_blocks);
}

void heap_free(struct heap *heap, void *ptr)
{
    heap_mark_blocks_free(heap, heap_addr_to_block(heap, ptr));
}
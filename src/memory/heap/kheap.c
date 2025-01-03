#include "kheap.h"
#include "heap.h"
#include "config.h"
#include "kernel.h"

struct heap kernel_heap;
struct heap_table kernel_heap_table;
// 1024 * 1024 * 100 = 104857600 (100MB)
// 104857600 / 4096 = 25600 heap entries

void kheap_init()
{
    int total_table_entries = CARBONOS_HEAP_SIZE_BYTES / CARBONOS_HEAP_BLOCK_SIZE;
    kernel_heap_table.entries = (HEAP_BLOCK_TABLE_ENTRY *)(CARBONOS_HEAP_TABLE_START_ADDR);
    kernel_heap_table.total = total_table_entries;

    void *end = (void *)(CARBONOS_HEAP_START_ADDR + CARBONOS_HEAP_SIZE_BYTES); // end of heap

    int res = heap_create(&kernel_heap, (void *)(CARBONOS_HEAP_START_ADDR), end, &kernel_heap_table);
    if (res < 0)
    {
        print("\nFailed to create heap\n");
    }
}

void *kmalloc(size_t size)
{
    return heap_malloc(&kernel_heap, size);
}

void kfree(void *ptr)
{
    return heap_free(&kernel_heap, ptr);
}
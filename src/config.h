#ifndef CONFIG_H
#define CONFIG_H

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10

// Amount of possible interrupts
#define CARBONOS_TOTAL_INTERRUPTS 512
// 100MB heap size
#define CARBONOS_HEAP_SIZE_BYTES 104857600
// Each block must have 4096 bytes
#define CARBONOS_HEAP_BLOCK_SIZE 4096
// Heap start address
#define CARBONOS_HEAP_START_ADDR 0x01000000
// Heap table start address
#define CARBONOS_HEAP_TABLE_START_ADDR 0x00007E00

#endif
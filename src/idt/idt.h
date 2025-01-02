#ifndef IDT_H
#define IDF_H

#include <stdint.h>

// https://wiki.osdev.org/Interrupt_Descriptor_Table

struct idt_desc
{
    uint16_t offset_1;  // offset 0-15
    uint16_t selector;  // 
    uint8_t zero;       // unuset set to zero
    uint8_t type_attr;  // descriptor attributes: P + DPL + S + Type
    uint16_t offset_2;  // offset 16-31
} __attribute__((packed)); // attribute to assure the positioning of the struct properties are maintained

struct idtr_desc
{
    uint16_t limit;     // size of desc table -1
    uint32_t base;      // itr dec table start base address
} __attribute__((packed)); // attribute to assure the positioning of the struct properties are maintained

void idt_init();
void enable_it();
void disable_it();

#endif
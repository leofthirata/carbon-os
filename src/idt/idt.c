#include "idt.h"
#include "config.h" 
#include "memory/memory.h" // memset
#include "kernel.h"
#include "io/io.h"

struct idt_desc idt_descriptors[CARBONOS_TOTAL_INTERRUPTS];
struct idtr_desc idtr_descriptor; 

extern void idt_load(struct idtr_desc *ptr);
extern void int21h();
extern void no_interrupt();

void int21h_handler()
{
    print("\nKeyboard IRQ\n");
    // send EOI - end of interrupt
    outb(0x20, 0x20);
}

void no_interrupt_handler()
{
    // send EOI - end of interrupt
    outb(0x20, 0x20);
}

void idt_zero()
{
    print("\nDivide by zero error\n");
}

// idx: interrupt number, addr: address in 64 bits
void idt_set(int idx, void *addr)
{
    struct idt_desc *desc = &idt_descriptors[idx];
    desc->offset_1 = (uint32_t) addr & 0x0000FFFF;
    desc->selector = KERNEL_CODE_SELECTOR;
    desc->zero = 0x00;
    desc->type_attr = 0xEE;
    // P + DPL + S + Type = 1 11 0 1110 (0xE) = 0xEE
    desc->offset_2 = (uint32_t) addr >> 16;
}

void idt_init()
{
    memset(idt_descriptors, 0, sizeof(idt_descriptors));
    idtr_descriptor.limit = sizeof(idt_descriptors) - 1;
    idtr_descriptor.base = (uint32_t) idt_descriptors;

    for (int i = 0; i < CARBONOS_TOTAL_INTERRUPTS; i++)
    {
        idt_set(i, no_interrupt);
    }

    idt_set(0, idt_zero);
    idt_set(0x21, int21h);

    idt_load(&idtr_descriptor);
}
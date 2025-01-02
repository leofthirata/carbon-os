[BITS 32]

global _start
global problem ; for testing purposes
global problem2 ; for testing purposes
extern kernel_main

CODE_SEG equ 0x08
DATA_SEG equ 0x10

_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov ebp, 0x00200000
    mov esp, ebp

    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; remap the master PIC
    mov al, 00010001b
    out 0x20, al    ; tell master pic

    mov al, 0x20    ; 0x20 is where irq should start
    out 0x21, al

    mov al, 00000001b
    out 0x21, al
    ; end remap

    ; sti             ; dangerous to do because it enables interupts before initialization of idt_init
    call kernel_main
    jmp $

problem2: ; causes divide by zero
    int 0

problem: ; causes divide by zero
    mov eax, 0
    div eax, 

times 512-($ - $$) db 0 ; fix aligment issues
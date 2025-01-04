[BITS 32]

; add-symbol-file ../build/kernelfull.o 0x100000
; gdb knows where we are loading kernel due to above config 0x100000

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

    ; Enable A20 line - Fast A20 Gate
    in al, 0x92
    or al, 2
    out 0x92, al

    ;https://en.wikibooks.org/wiki/X86_Assembly/Programmable_Interrupt_Controller
    ; remap the master PIC (program interrupt controller)
    mov al, 00010001b ; 0x11 write ICW1 to PICM, we are gonna write commands to PICM
    out 0x20, al    ; restart pic

    mov al, 0x20    ; 
    out 0x21, al    ; remap PICM to 0x20 (32 decimal)

    mov al, 00000001b   ; write ICW4 to PICM, we are gonna write commands to PICM
    out 0x21, al        ;
    ; end remap

    ; sti             ; dangerous to do because it enables interupts before initialization of idt_init
    call kernel_main  ; call kernel main function in kernel.c file
    jmp $

problem2: ; causes divide by zero
    int 0

problem: ; causes divide by zero
    mov eax, 0
    div eax, 

times 512-($ - $$) db 0 ; fix aligment issues
section .asm

extern int21h_handler
extern no_interrupt_handler

global int21h
global no_interrupt
global idt_load
global enable_it
global disable_it

enable_it:
    sti
    ret

disable_it:
    cli
    ret

idt_load:
    push ebp        ; base pointer
    mov ebp, esp    ; move stack poitner to base pointer

    mov ebx, [ebp+8] ; without +8, it would point to base point
                    ; +4 points to return addr of the caller function
    lidt [ebx]      ; lidt Load Global/Interrupt Descriptor Table, loads idt received as param in ebp+8
    pop ebp         ; restore address ebp was pointing to
    ret

int21h:
    cli
    pushad          ; saves all gpr

    call int21h_handler

    popad           ; restores all gpr
    sti
    iret

no_interrupt:
    cli
    pushad          ; saves all gpr

    call no_interrupt_handler

    popad           ; restores all gpr
    sti
    iret
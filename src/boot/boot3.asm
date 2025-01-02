; osdev.org for exceptions

ORG 0 ; starting addr
BITS 16 ; only 16 bit code

_start:
    jmp short start
    nop

times 33 db 0 ; creates 33 zeroed bytes after short jump

start:
    jmp 0x7c0:step2

handle_zero:
    mov ah, 0eh
    mov al, 'A'
    mov bx, 0x00
    int 0x10
    iret ; interrupt return

handle_one:
    mov ah, 0eh
    mov al, 'V'
    mov bx, 0x00
    int 0x10
    iret ; interrupt return

step2:    
    cli ; clear interrupts
    mov ax, 0x7c0
    mov ds, ax
    mov es, ax ; 0x7c0 to es
    
    mov ax, 0x00
    mov ss, ax
    mov sp, 0x7c00

    sti ; enables interrupts

    mov word[ss:0x00], handle_zero ; specify ss segment which points at zero
    mov word[ss:0x02], 0x7c0
    int 0 ; calls interrupt zero

    mov word[ss:0x04], handle_one ; specify ss segment which points at zero
    mov word[ss:0x06], 0x7c0
    int 1 ; calls interrupt zero

    mov si, message ; message -> si
    call print ; call print function
    jmp $

print:
    mov bx, 0
.loop:
    lodsb ; increments si index which points to message
    cmp al, 0 ; if eq to 0, go to done
    je .done
    call print_char
    jmp .loop
.done:
    ret ; return

print_char:
    mov ah, 0eh ; 0eh to ah reg terminal command
    int 0x10 ; calling bios routine
    ret

message: db 'Carbon OS greets you!', 0

times 510-($ - $$) db 0 ; fills 510 of data
dw 0xAA55 ; 55AA
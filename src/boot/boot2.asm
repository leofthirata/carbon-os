; https://ctyme.com/intr/rb-0106.htm
; sets correct pointers and address to assure it works

ORG 0 ; starting addr
BITS 16 ; only 16 bit code

_start:
    jmp short start
    nop

times 33 db 0 ; creates 33 zeroed bytes after short jump

start:
    jmp 0x7c0:step2

step2:    
    cli ; clear interrupts
    mov ax, 0x7c0
    mov ds, ax
    mov es, ax ; 0x7c0 to es
    
    mov ax, 0x00
    mov ss, ax
    mov sp, 0x7c00

    sti ; enables interrupts
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
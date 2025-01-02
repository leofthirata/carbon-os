; https://ctyme.com/intr/rb-0106.htm

ORG 0x7c00 ; starting addr
BITS 16 ; only 16 bit code

start:
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
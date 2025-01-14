section .asm

global insb
global insw
global outb
global outw

insb:                   ; byte input
    push ebp
    mov ebp, esp
    
    xor eax, eax        ; eax equals zero
    mov edx, [ebp+8]    ; gets the lower 8 bits
    in al, dx           ; https://c9x.me/x86/html/file_module_x86_id_139.html

    pop ebp
    ret

insw:                   ; word input
    push ebp
    mov ebp, esp

    xor eax, eax
    mov edx, [ebp+8]
    in ax, dx

    pop ebp
    ret

outb:
    push ebp
    mov ebp, esp

    mov eax, [ebp+12]   ; second param
    mov edx, [ebp+8]    ; first param
    out dx, al

    pop ebp
    ret

outw:
    push ebp
    mov ebp, esp

    mov eax, [ebp+12]
    mov edx, [ebp+8]
    out dx, ax

    pop ebp
    ret
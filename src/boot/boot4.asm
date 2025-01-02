; DISK - READ SECTOR(S) INTO MEMORY
; AH = 02h
; AL = number of sectors to read (must be nonzero)
; CH = low eight bits of cylinder number
; CL = sector number 1-63 (bits 0-5)
; high two bits of cylinder (bits 6-7, hard disk only)
; DH = head number
; DL = drive number (bit 7 set for hard disk)
; ES:BX -> data buffer


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

    ; the code below sets the interrupt 0x13 to read data from sector
    ; the bx will point to buffer so the data read from the sector
    ; will be stored there
    ; then buffer to si and print the message 
    mov ah, 2
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov bx, buffer

    int 0x13
    jc error

    mov si, buffer
    call print

    jmp $

error:
    mov si, error_message
    call print
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

error_message: db 'Failed to load sector', 0

times 510-($ - $$) db 0 ; fills 510 of data
dw 0xAA55 ; 55AA

buffer:

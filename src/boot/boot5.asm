; https://wiki.osdev.org/Protected_Mode

ORG 0x7c00 ; starting addr
BITS 16 ; only 16 bit code

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
_start:
    jmp short start
    nop

times 33 db 0 ; creates 33 zeroed bytes after short jump

start:
    jmp 0:step2

step2:    
    cli ; clear interrupts
    mov ax, 0x00    ; ax general purpose reg
    mov ds, ax      ; ds data seg
    mov es, ax ; 0x7c0 to es extra seg
    mov ss, ax      ; stack seg
    mov sp, 0x7c00  ; stack pointer
    sti ; enables interrupts

.load_protected:    ; local label
    cli             ; clear itr
    lgdt[gdt_descriptor] ; gdt global descriptor table pointer 
    mov eax, cr0    ; eax 32 bit gpr
    or eax, 0x1     ; or logic operator
    mov cr0, eax    ; cr0 control reg https://wiki.osdev.org/CPU_Registers_x86#CR0
    jmp CODE_SEG:load32
    ; jmp $

; GDT
gdt_start:
gdt_null:
    dd 0x0      ; dd pseudo instruction
    dd 0x0      

; offset 0x8
gdt_code:       ; CS shoudl point to this
    dw 0xffff  ; segment first 0-15 bits
    dw 0        ; base first 0-15 bits
    db 0        ; base 16-23 bits
    db 0x9a     ; access byte
    db 11001111b ; flag
    db 0        ; base 24-31 bits

; offset 0x10
gdt_data:       ; DS, SS, ES, FS, GS
    dw 0xffff  ; segment first 0-15 bits
    dw 0        ; base first 0-15 bits
    db 0        ; base 16-23 bits
    db 0x92     ; access byte
    db 11001111b ; flag
    db 0        ; base 24-31 bits

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32] ; everything below this line is considered as 32bits
load32:
    mov eax, 1          ; starting sector, 0 is boot sector
    mov ecx, 100        ; ecx 32 bit gpr, amount of sectors to be loaded
    mov edi, 0x0100000  ; edi 32 bit gpr, address to load into
    call ata_lba_read
    jmp CODE_SEG:0x0100000

ata_lba_read:           ; logical block addressing read
    mov ebx, eax,       ; backup the lba
    ; send the lba highest 8 bits to hard disk controller
    shr eax, 24         ; shift 24 bits to right and stores in eax
    or eax, 0xE0        ; or eax with 0xE0 to select master drive
    mov dx, 0x1F6       ; port to write 8 bits
    out dx, al          ; al reg is 8 bits
    ; send done

    ; send the 8 bits to 0x1F2
    mov eax, ecx
    mov dx, 0x1F2 
    out dx, al
    ; done

    ; send the 8 bits to 0x1F3
    mov eax, ebx ; restore back lda
    mov dx, 0x1F3
    out dx, al
    ; done

    ; send the 8 bits to 0x1F4
    mov dx, 0x1F4
    mov eax, ebx ; restore backup lda
    shr eax, 8
    out dx, al
    ; done

    ; send lda upper 16 bits
    mov dx, 0x1F5
    mov eax, ebx
    shr eax, 16
    out dx, al
    ; done

    mov dx, 0x1F7
    mov al, 0x20
    out dx, al

;read all sectors into memory
.next_sector:
    push ecx

; check if need to read
.try_again:
    mov dx, 0x1F7
    in al, dx
    test al, 8
    jz .try_again ; 

; read 256 words at a time
    mov ecx, 256
    mov dx, 0x1F0
    rep insw ; store 0x1F0 (dx) to edi
    pop ecx 
    loop .next_sector
; done reading
    ret


times 510-($ - $$) db 0 ; fills 510 of data
dw 0xAA55 ; 55AA

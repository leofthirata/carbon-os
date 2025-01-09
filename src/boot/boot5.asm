; https://wiki.osdev.org/Protected_Mode

ORG 0x7c00 ; starting addr
BITS 16 ; only 16 bit code

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; byte offset 0x00 and 3 bytes length
jmp short start
nop

; FAT16 Header

; offset 0x03 len 8 bytes
OEMIdentifier       db 'CARBONOS'
; offset 0x0B len 2 bytes
BytesPerSector      dw 0x200 ; 512 bytes per sector
; offset 0x0D len 1 byte
SectorsPerCluster   db 0x80
; offset 0x0E len 2 bytes
ReservedSectors     dw 200 ; kernel will be stored here
; offset 0x10 len 1 byte
FATCopies           db 0x02 ; original and backup
; offset 0x11 len 2 bytes
RootDirEntries      dw 0x40 ; amount of root directory entries
; offset 0x13 len 2 bytes
NumSectors          dw 0x00 ; total sectors in logical volume; 0 means more than 65535 sectors 
; offset 0x15 len 1 byte
MediaType           db 0xF8 ; media descriptor type
; offset 0x16 len 2 bytes
SectorsPerFat       dw 0x100 ; FAT12/FAT16 only
; offset 0x18 len 2 bytes
SectorsPerTrack     dw 0x20
; offset 0x1A len 2 bytes
NumberOfHeads       dw 0x40 ; heads or sides on storage media
; offset 0x1C len 4 bytes
HiddenSectors       dd 0x00
; offset 0x20 len 4 bytes
SectorsBig          dd 0x773594 ; must be set if NumSectors is 0

; Extended Boot Record (EBPB)
; 0x24 len 1
DriveNumber         db 0x80 ; 0x80 for hard disks
; 0x25 len 1
WinNTBit            db 0x00 ; flags in windows nt
; 0x26 len 1
Signature           db 0x29 ; must be 0x28 or 0x29
; 0x27 len 4
VolumeID            dd 0xD105 ; serial number
; 0x2B len 11
VolumeIDString      db 'CARBON BOOT' ; volume label string
; 0x36 len 8
SystemIDString      db 'FAT16   ' ; system identifier string
; 0x3E len 448
; 0x1FE len 2       dw 0x55AA


;line below is to reserve space for fat header in future
;times 33 db 0 ; creates 33 zeroed bytes after short jump

start:
    jmp 0:step2

step2:    
    cli ; clear interrupts
    mov ax, 0x00    ; ax general purpose reg
    mov ds, ax      ; ds data seg
    mov es, ax      ; es extra seg
    mov ss, ax      ; stack seg
    mov sp, 0x7c00  ; stack pointer
    sti ; enables interrupts

.load_protected:    ; local label
    cli             ; clear itr
    lgdt[gdt_descriptor] ; gdt global descriptor table pointer 

    ; set PE (Protection Enable) bit in CR0 (Control Register 0)
    mov eax, cr0    ; eax 32 bit gpr
    or eax, 0x1     ; or logic operator
    mov cr0, eax    ; cr0 control reg https://wiki.osdev.org/CPU_Registers_x86#CR0
    jmp CODE_SEG:load32
    ; jmp $

; GDT - 32 bit Kernel Setup
gdt_start:
gdt_null:
    dd 0x0      ; dd pseudo instruction
    dd 0x0      

; offset 0x8 limit 0xffff access byte 0x9a flags 0xc
gdt_code:       ; kernel code segment shoudl point to this
    dw 0xffff   ; word limit 0-15 bits
    dw 0        ; word base 16-31 bits
    db 0        ; byte base 32-39 bits
    db 0x9a     ; byte access byte 40-47
    db 11001111b; flag 48-55
    db 0        ; byte base 56-63 bits

; offset 0x10 limit 0xffff access byte 0x92 flags 0xc
gdt_data:       ; kernel data segment shoudl point to this
    dw 0xffff   ; word limit 0-15 bits
    dw 0        ; word base 16-31 bits
    db 0        ; byte base 32-39 bits
    db 0x92     ; byte access byte 40-47
    db 11001111b; flag 48-55
    db 0        ; byte base 56-63 bits

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32] ; everything below this line is considered as 32bits
load32: ; load kernel into memory and jump into it
    mov eax, 1          ; starting sector, 0 is boot sector
    mov ecx, 100        ; ecx 32 bit gpr, amount of sectors of nulls to be loaded
    mov edi, 0x0100000  ; edi 32 bit destination reg, address to load into, 1MB
    call ata_lba_read
    jmp CODE_SEG:0x0100000 ; jump to kernel starting addr

ata_lba_read:           ; logical block addressing read load sectors into memory
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


times 510-($ - $$) db 0 ; fills 510 bytes of data
dw 0xAA55 ; 55AA to binary file as the end flag

# Linux Kernel Development

## Memory
- information storage
- RAM: random access memory, main computer memory, read write, temporary storage, data lost after power down
- ROM: read only memory, permanent storage and maintain data after power down, BIOS is stored in a ROM

## Boot proccess
1. BIOS -> Bootloader
2. BOOTLOADER into address 0x7c00 -> Kernel
3. Kernel

### Bootloader
- responsible for loading kernel
- small and limited memory size

### BIOS
- executed from ROM
- loads itself into RAM
- initialize essential hardware
- looks for bootloader to boot searching for boot signature 0x55AA
- is like a kernel itself
- 16 bits

## Needed components for kernel development
1. sudo apt update
2. sudo apt install nasm
3. sudo apt install qemu-system-x86
4. qemu-system-x86_64

## Real Mode
- compability mode which all intel starts when switched on
- access only 1Mb RAM
- based on x86 design; acts like 1970 processors and limited just like them
- no security in memory and hardware
- 8 and 16 bits

## Memory model

- cs code segment
- ss stack segment
- ds data segment
- es extra segment

lodsb uses DS:SI register combination ; DS is the segment so DS * 16 + offset SI
org 0
mov ax, 0x7c0
mov ds, ax ; multiply 0x7c0 by 16
mov si, 0x1F ; adds 0x1F resulting in 0x7C1F
lodsb

0x7C0 * 16 = 0x7C00
0x7C00 + 0x1F = 0x7C1F

### stack segment
SS = 0x00
SP (stack pointer) = 0x7C00
push 0xFFEE ; decrements SP by 2, SP = 0x7BFE then
data from 0x7BFE to 0x7BFF is 0xFFEE

## Burn bootloader to a usb stick
1. sudo fdisk -l
2. sudo dd if=./boot.bin of=/dev/sdb
if input file
of output file

## Interrupt
- doesnt need to know address to call it
- interrupt vector table IVT describes 256 interrupt handlers
- offset + segment = 4 bytes each interrupt
- interrupt 0x13 -> 0x13 * 0x04 = 0x46 or 76 -> 76-77 offset 78-79 segment

## Filesystem
- filesystems are needed to understand files format
- data is read and written in 512 byte blocks

### CHS cylinder head sector
- a track to specify the sector

### LBA logical block address
- specify a number that starts from zero
- LBA 0 -> first sector LBA 1 -> second sector
- read byte at 58376 position -> LBA = 58376 / 512 -> Offset 58376 % 512 = 8 -> sector 114 * 512 + offset 8

## Makefile
sudo apt install make
- run first label seen
sudo apt install bless

## Protected Mode
- memory protection
- 4gb memory
- protects memory and hardware from being accessed
- rings with different priviledges
- 32 bits
- once in protected mode, cant use bios anymore

### Paging memory scheme
- virtual addr points to physical addr

sudo apt install gdb
target remote | qemu-system-x86_64 -hda ./boot.bin -S -gdb stdio
enter "c" to continue
press ctrl C
info registers
if cs 0x8 is set to 8, then its in protected mode

## A20 Line
needs to be enabled to access bit 20

## Cross compiler for C code
https://wiki.osdev.org/GCC_Cross-Compiler

sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libcloog-isl-dev libisl-dev

### Install binutils
https://ftp.gnu.org/gnu/binutils/

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

cd $HOME/src

mkdir build-binutils
cd build-binutils
../binutils-x.y.z/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install

### Install GCC

cd gcc-10.2.0
./contrib/download_prerequisites

cd $HOME/src

which -- $TARGET-as || echo $TARGET-as is not in the PATH

mkdir build-gcc
cd build-gcc
../gcc-x.y.z/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc
make all-target-libgcc
make all-target-libstdc++-v3 # might not be needed
make install-gcc
make install-target-libgcc
make install-target-libstdc++-v3 # might not be needed

### build.sh

create build.sh inside OS folder

<!-- inside build.sh -->
#/bin/bash

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

make all
<!-- inside build.sh -->

sudo chmod +x ./build.sh

gdb
add-symbol-file ../build/kernelfull.o 0x100000
break _start to add breakpoint
target remote | qemu-system-x86_64 -S -gdb stdio -hda ./os.bin
layout asm
stepi to step debugger

hda means hard driver

## C Code development begin

after creating kernel.c and kernel.h, add them to Makefile to compile and generate output files

flags
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

make clean
./build.sh
inside bin
gdb
add-symbol-file ../build/kernelfull.o 0x100000
target remote | qemu-system-x86_64 -hda ./os.bin -gdb stdio -S
break kernel_start
press C
bt for backtrace
layout asm
stepi

## Text Mode

- allows ascii to video memory
- each char takes two bytes -> byte 0 - ascii, byte 1 - colour code

qemu-system-x86_64 -hda ./os.bin

## Interrupt descriptor table
- for protected mode
- debugging kernel specific line code
make clean
./build.sh
gdb
add-symbol-file ./build/kernelfull.o 0x100000
target remote | qemu-system-x86_64 -hda ./bin/os.bin -gdb stdio -S
break kernel.c:<line>
c
layout asm
stepi
print $eax
print $edx

## Hardware Interrupt
- timer
- keyboard
- cascade (never used)
- COMs
- free for peripherals
- RTC
- FPU
- PS2 mouse
- seconadry ATA Disk
- IRQ 0-7 are master handles
- IRQ 8-15 are slave handles

## Heap memory
https://wiki.osdev.org/Memory_Map_(x86)

- heap memory implementations are memory managers
- memory limits for 32 bit kernel
can only have 4.29GB of ram since its 32 bits
- who uses ram: video, hardware
- 0x01000000 starting addr of uninitialized memory
- one type of implementation:
defined by block chunks of 4096 (the least possible value)
if we want 100MB of heap, then
100MB / 4096 = 25600 entries
then, 
entry 0: addr 0x01000000
entry 1: addr 0x01001000 starting addr + 4096 bytes
entry 2: addr 0x01002000
entry 3: addr 0x01003000
. 
.
.
entry flags bit mask
7       6      5 4 3   2   1   0
has_n is_first 0 0 et3 et2 et1 et0
has_n set if entry to the right of us is part of our allocation, so the last block of allocation will be reset
is_first set if first entry
two entry types: entry taken and entry free

### malloc example
- start addr 0x01000000
- assume heap 100MB

to test the implementation,
make clean
./build.sh
gdb
target remote | qemu-system-i386 -hda ./bin/os.bin -gdb stdio -S -> i386 because its 32 bits to avoid unexpected behaviours
add-symbol-file ./build/kernelfull.o 0x100000
break kernel.c:<line where malloc is used>
c
print ptr
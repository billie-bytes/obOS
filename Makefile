# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2025
DISK_NAME     = storage

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386
 
start: disk insert-all build run

run: all
# 	@qemu-system-i386 -s -S -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso
	qemu-system-i386 -s -S -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso
	
all: build
build: iso

# Insert all user programs into disk
insert-all: insert-shell insert-clock insert-hello insert-badapple insert-anim
	@echo All user programs inserted successfully!

clean:
	rm -rf *.o *.iso $(OUTPUT_FOLDER)/kernel

kernel:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/kernel-entrypoint.s -o $(OUTPUT_FOLDER)/kernel-entrypoint.o
# TODO: Compile C file with CFLAGS
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/kernel.c -o $(OUTPUT_FOLDER)/kernel.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/gdt.c -o $(OUTPUT_FOLDER)/gdt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/framebuffer.c -o $(OUTPUT_FOLDER)/framebuffer.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/interrupt.c -o $(OUTPUT_FOLDER)/interrupt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/idt.c -o $(OUTPUT_FOLDER)/idt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/portio.c -o $(OUTPUT_FOLDER)/portio.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/intsetup.s -o $(OUTPUT_FOLDER)/intsetup.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/keyboard.c -o $(OUTPUT_FOLDER)/keyboard.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/paging.c -o $(OUTPUT_FOLDER)/paging.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/disk.c -o $(OUTPUT_FOLDER)/disk.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/ext2.c -o $(OUTPUT_FOLDER)/ext2.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/string.c -o $(OUTPUT_FOLDER)/string.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/process.c -o $(OUTPUT_FOLDER)/process.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/scheduler.c -o $(OUTPUT_FOLDER)/scheduler.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cmos.c -o $(OUTPUT_FOLDER)/cmos.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/context-switch.s -o $(OUTPUT_FOLDER)/context-switch.o


	@$(LIN) $(LFLAGS) bin/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf32...
	@rm -f *.o

iso: kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel     $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1                 $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst   $(OUTPUT_FOLDER)/iso/boot/grub/
# TODO: Create ISO image
	@cd $(OUTPUT_FOLDER) && genisoimage -R			   \
	-b boot/grub/grub1         \
	-no-emul-boot              \
	-boot-load-size 4          \
	-A os                      \
	-input-charset utf8        \
	-quiet                     \
	-boot-info-table           \
	-o OS2025.iso              \
	iso
	@rm -r $(OUTPUT_FOLDER)/iso/

DISK_NAME = storage

disk:
	@qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M

inserter:
	@$(CC) -Wno-builtin-declaration-mismatch -g -I$(SOURCE_FOLDER) \
		$(SOURCE_FOLDER)/string.c \
		$(SOURCE_FOLDER)/ext2.c \
		$(SOURCE_FOLDER)/external-inserter.c \
		-o $(OUTPUT_FOLDER)/inserter

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c -o user-shell.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell
	@echo Linking object shell object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell_elf
	@echo Linking object shell object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

insert-shell: inserter user-shell
	@echo Inserting shell into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter shell 2 $(DISK_NAME).bin

user-clock:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-clock.c -o user-clock.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-clock.o -o $(OUTPUT_FOLDER)/clock
	@echo Linking object clock object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/clock
	@rm -f *.o

insert-clock: inserter user-clock
	@echo Inserting clock into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter clock 2 $(DISK_NAME).bin

user-hello:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-hello.c -o user-hello.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-hello.o string.o -o $(OUTPUT_FOLDER)/hello
	@echo Linking object hello object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/hello
	@rm -f *.o

insert-hello: inserter user-hello
	@echo Inserting hello into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter hello 2 $(DISK_NAME).bin

user-badapple:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-badapple.c -o user-badapple.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-badapple.o string.o -o $(OUTPUT_FOLDER)/badapple
	@echo Linking badapple object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/badapple
	@rm -f *.o

insert-badapple: inserter user-badapple
	@echo Inserting badapple into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter badapple 2 $(DISK_NAME).bin

insert-anim: inserter
	@echo Inserting badapple.txt into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter badapple.txt 2 $(DISK_NAME).bin

insert-file: inserter
	@echo Inserting $(FILE_NAME) into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter $(FILE_NAME) 2 $(DISK_NAME).bin
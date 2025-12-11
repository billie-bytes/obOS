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
	qemu-system-i386 -s -S -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso
all: build
build: iso

# Insert all user programs into disk
insert-all: insert-shell insert-clock insert-hello insert-badapple insert-anim insert-commands insert-beep
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
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/speaker.c -o $(OUTPUT_FOLDER)/speaker.o
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
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/clock/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/clock/clock.c -o clock.o
	@$(LIN) -T $(SOURCE_FOLDER)/clock/linker.ld -melf_i386 --oformat=binary \
		crt0.o clock.o -o $(OUTPUT_FOLDER)/clock
	@echo Linking clock object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/clock
	@rm -f *.o

insert-clock: inserter user-clock
	@echo Inserting clock into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter clock 2 $(DISK_NAME).bin

user-hello:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/hello/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/hello/hello.c -o hello.o
	@$(LIN) -T $(SOURCE_FOLDER)/hello/linker.ld -melf_i386 --oformat=binary \
		crt0.o hello.o -o $(OUTPUT_FOLDER)/hello
	@echo Linking hello object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/hello
	@rm -f *.o

insert-hello: inserter user-hello
	@echo Inserting hello into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter hello 2 $(DISK_NAME).bin

user-beep:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-beep.c -o user-beep.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o	
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-beep.o string.o -o $(OUTPUT_FOLDER)/beep
	@echo Linking object beep object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/beep
	@rm -f *.o

insert-beep: inserter user-beep
	@echo Inserting beep into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter beep 2 $(DISK_NAME).bin
user-badapple:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/badapple/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/badapple/badapple.c -o badapple.o
	@$(LIN) -T $(SOURCE_FOLDER)/badapple/linker.ld -melf_i386 --oformat=binary \
		crt0.o badapple.o -o $(OUTPUT_FOLDER)/badapple
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

# External commands
CMD_LINKER = $(SOURCE_FOLDER)/command/cmd-linker.ld
CMD_CRT0 = $(SOURCE_FOLDER)/command/crt0-cmd.s

cmd-pwd: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_pwd.c -o cmd_pwd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_pwd.o string.o -o $(OUTPUT_FOLDER)/pwd
	@echo Built pwd command
	@cd $(OUTPUT_FOLDER); ./inserter pwd 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-clear: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_clear.c -o cmd_clear.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_clear.o -o $(OUTPUT_FOLDER)/clear
	@echo Built clear command
	@cd $(OUTPUT_FOLDER); ./inserter clear 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-help: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_help.c -o cmd_help.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_help.o string.o -o $(OUTPUT_FOLDER)/help
	@echo Built help command
	@cd $(OUTPUT_FOLDER); ./inserter help 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-exit: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_exit.c -o cmd_exit.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_exit.o -o $(OUTPUT_FOLDER)/exit
	@echo Built exit command
	@cd $(OUTPUT_FOLDER); ./inserter exit 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-echo: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_echo.c -o cmd_echo.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_echo.o string.o -o $(OUTPUT_FOLDER)/echo
	@echo Built echo command
	@cd $(OUTPUT_FOLDER); ./inserter echo 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-ls: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_ls.c -o cmd_ls.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_ls.o string.o -o $(OUTPUT_FOLDER)/ls
	@echo Built ls command
	@cd $(OUTPUT_FOLDER); ./inserter ls 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-cd: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_cd.c -o cmd_cd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_cd.o string.o -o $(OUTPUT_FOLDER)/cd
	@echo Built cd command
	@cd $(OUTPUT_FOLDER); ./inserter cd 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-cat: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_cat.c -o cmd_cat.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_cat.o string.o -o $(OUTPUT_FOLDER)/cat
	@echo Built cat command
	@cd $(OUTPUT_FOLDER); ./inserter cat 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-mkdir: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_mkdir.c -o cmd_mkdir.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_mkdir.o string.o -o $(OUTPUT_FOLDER)/mkdir
	@echo Built mkdir command
	@cd $(OUTPUT_FOLDER); ./inserter mkdir 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-ps: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_ps.c -o cmd_ps.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_ps.o string.o -o $(OUTPUT_FOLDER)/ps
	@echo Built ps command
	@cd $(OUTPUT_FOLDER); ./inserter ps 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-kill: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_kill.c -o cmd_kill.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_kill.o string.o -o $(OUTPUT_FOLDER)/kill
	@echo Built kill command
	@cd $(OUTPUT_FOLDER); ./inserter kill 2 $(DISK_NAME).bin
	@rm -f *.o

cmd-exec: inserter
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_exec.c -o cmd_exec.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary \
		crt0-cmd.o cmd_exec.o string.o -o $(OUTPUT_FOLDER)/exec
	@echo Built exec command
	@cd $(OUTPUT_FOLDER); ./inserter exec 2 $(DISK_NAME).bin
	@rm -f *.o

insert-commands: cmd-pwd cmd-clear cmd-help cmd-exit cmd-echo cmd-ls cmd-cd cmd-cat cmd-mkdir cmd-ps cmd-kill cmd-exec
	@echo All external commands inserted!

.PHONY: cmd-pwd cmd-clear cmd-help cmd-exit cmd-echo cmd-ls cmd-cd cmd-cat cmd-mkdir cmd-ps cmd-kill cmd-exec insert-commands

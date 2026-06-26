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
#WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386
 
all: start

start: disk build insert-all run

build: kernel inserter user-programs build-commands iso

run:
	qemu-system-i386 -s -S -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso

clean:
	rm -rf *.o *.iso $(OUTPUT_FOLDER)/kernel
	rm -f $(OUTPUT_FOLDER)/shell $(OUTPUT_FOLDER)/shell_elf $(OUTPUT_FOLDER)/clock $(OUTPUT_FOLDER)/hello $(OUTPUT_FOLDER)/beep $(OUTPUT_FOLDER)/badapple $(OUTPUT_FOLDER)/inserter
	rm -f $(OUTPUT_FOLDER)/pwd $(OUTPUT_FOLDER)/clear $(OUTPUT_FOLDER)/help $(OUTPUT_FOLDER)/exit $(OUTPUT_FOLDER)/echo $(OUTPUT_FOLDER)/ls $(OUTPUT_FOLDER)/cd $(OUTPUT_FOLDER)/cat $(OUTPUT_FOLDER)/mkdir $(OUTPUT_FOLDER)/mkdir_elf $(OUTPUT_FOLDER)/ps $(OUTPUT_FOLDER)/kill $(OUTPUT_FOLDER)/exec $(OUTPUT_FOLDER)/grep $(OUTPUT_FOLDER)/find
	rm -f $(OUTPUT_FOLDER)/$(DISK_NAME).bin $(OUTPUT_FOLDER)/$(ISO_NAME).iso

kernel:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/kernel-entrypoint.s -o $(OUTPUT_FOLDER)/kernel-entrypoint.o
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
	@cp $(OUTPUT_FOLDER)/kernel $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1 $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst $(OUTPUT_FOLDER)/iso/boot/grub/
	@cd $(OUTPUT_FOLDER) && genisoimage -R -b boot/grub/grub1 -no-emul-boot -boot-load-size 4 -A os -input-charset utf8 -quiet -boot-info-table -o OS2025.iso iso
	@rm -r $(OUTPUT_FOLDER)/iso/

disk:
	@dd if=/dev/zero of=$(OUTPUT_FOLDER)/$(DISK_NAME).bin bs=1M count=1024

inserter:
	@$(CC) -Wno-builtin-declaration-mismatch -g -I$(SOURCE_FOLDER) \
		$(SOURCE_FOLDER)/string.c \
		$(SOURCE_FOLDER)/ext2.c \
		$(SOURCE_FOLDER)/external-inserter.c \
		-o $(OUTPUT_FOLDER)/inserter

user-programs: user-shell user-clock user-hello user-beep user-badapple

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c -o user-shell.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell_elf
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

user-clock:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/clock/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/clock/clock.c -o clock.o
	@$(LIN) -T $(SOURCE_FOLDER)/clock/linker.ld -melf_i386 --oformat=binary crt0.o clock.o -o $(OUTPUT_FOLDER)/clock
	@size --target=binary $(OUTPUT_FOLDER)/clock
	@rm -f *.o

user-hello:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/hello/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/hello/hello.c -o hello.o
	@$(LIN) -T $(SOURCE_FOLDER)/hello/linker.ld -melf_i386 --oformat=binary crt0.o hello.o -o $(OUTPUT_FOLDER)/hello
	@size --target=binary $(OUTPUT_FOLDER)/hello
	@rm -f *.o

user-beep:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-beep.c -o user-beep.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o    
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary crt0.o user-beep.o string.o -o $(OUTPUT_FOLDER)/beep
	@size --target=binary $(OUTPUT_FOLDER)/beep
	@rm -f *.o

user-badapple:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/badapple/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/badapple/badapple.c -o badapple.o
	@$(LIN) -T $(SOURCE_FOLDER)/badapple/linker.ld -melf_i386 --oformat=binary crt0.o badapple.o -o $(OUTPUT_FOLDER)/badapple
	@size --target=binary $(OUTPUT_FOLDER)/badapple
	@rm -f *.o

CMD_LINKER = $(SOURCE_FOLDER)/command/cmd-linker.ld
CMD_CRT0 = $(SOURCE_FOLDER)/command/crt0-cmd.s

build-commands: cmd-pwd cmd-clear cmd-help cmd-echo cmd-ls cmd-cat cmd-mkdir cmd-ps cmd-kill cmd-exec cmd-grep cmd-find

cmd-pwd:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_pwd.c -o cmd_pwd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_pwd.o string.o -o $(OUTPUT_FOLDER)/pwd
	@rm -f *.o

cmd-clear:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_clear.c -o cmd_clear.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_clear.o -o $(OUTPUT_FOLDER)/clear
	@rm -f *.o

cmd-help:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_help.c -o cmd_help.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_help.o string.o -o $(OUTPUT_FOLDER)/help
	@rm -f *.o

cmd-echo:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_echo.c -o cmd_echo.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_echo.o string.o -o $(OUTPUT_FOLDER)/echo
	@rm -f *.o

cmd-ls:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_ls.c -o cmd_ls.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_ls.o string.o -o $(OUTPUT_FOLDER)/ls
	@rm -f *.o



cmd-cat:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_cat.c -o cmd_cat.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_cat.o string.o -o $(OUTPUT_FOLDER)/cat
	@rm -f *.o

cmd-mkdir:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_mkdir.c -o cmd_mkdir.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_mkdir.o string.o -o $(OUTPUT_FOLDER)/mkdir
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=elf32-i386 crt0-cmd.o cmd_mkdir.o string.o -o $(OUTPUT_FOLDER)/mkdir_elf
	@rm -f *.o

cmd-ps:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_ps.c -o cmd_ps.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_ps.o string.o -o $(OUTPUT_FOLDER)/ps
	@rm -f *.o

cmd-kill:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_kill.c -o cmd_kill.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_kill.o string.o -o $(OUTPUT_FOLDER)/kill
	@rm -f *.o

cmd-exec:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_exec.c -o cmd_exec.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_exec.o string.o -o $(OUTPUT_FOLDER)/exec
	@rm -f *.o

cmd-grep:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_grep.c -o cmd_grep.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_grep.o string.o -o $(OUTPUT_FOLDER)/grep
	@rm -f *.o

cmd-find:
	@$(ASM) $(AFLAGS) $(CMD_CRT0) -o crt0-cmd.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/command/cmd_find.c -o cmd_find.o
	@$(CC) $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(CMD_LINKER) -melf_i386 --oformat=binary crt0-cmd.o cmd_find.o string.o -o $(OUTPUT_FOLDER)/find
	@rm -f *.o


insert-all: insert-shell insert-clock insert-hello insert-badapple insert-anim insert-commands insert-beep
	@echo All programs inserted!

insert-shell: inserter user-shell
	@cd $(OUTPUT_FOLDER); ./inserter shell 2 $(DISK_NAME).bin

insert-clock: inserter user-clock
	@cd $(OUTPUT_FOLDER); ./inserter clock 2 $(DISK_NAME).bin

insert-hello: inserter user-hello
	@cd $(OUTPUT_FOLDER); ./inserter hello 2 $(DISK_NAME).bin

insert-beep: inserter user-beep
	@cd $(OUTPUT_FOLDER); ./inserter beep 2 $(DISK_NAME).bin

insert-badapple: inserter user-badapple
	@cd $(OUTPUT_FOLDER); ./inserter badapple 2 $(DISK_NAME).bin

insert-anim: inserter
	@cd $(OUTPUT_FOLDER); ./inserter badapple.txt 2 $(DISK_NAME).bin

insert-file: inserter
	@cd $(OUTPUT_FOLDER); ./inserter $(FILE_NAME) 2 $(DISK_NAME).bin

insert-commands: inserter build-commands
	@echo Inserting external commands into root directory...
	@cd $(OUTPUT_FOLDER); \
	./inserter pwd 2 $(DISK_NAME).bin; \
	./inserter clear 2 $(DISK_NAME).bin; \
	./inserter help 2 $(DISK_NAME).bin; \
	./inserter echo 2 $(DISK_NAME).bin; \
	./inserter ls 2 $(DISK_NAME).bin; \
	./inserter cat 2 $(DISK_NAME).bin; \
	./inserter mkdir 2 $(DISK_NAME).bin; \
	./inserter ps 2 $(DISK_NAME).bin; \
	./inserter kill 2 $(DISK_NAME).bin; \
	./inserter exec 2 $(DISK_NAME).bin; \
	./inserter grep 2 $(DISK_NAME).bin; \
	./inserter find 2 $(DISK_NAME).bin

.PHONY: all start build clean user-programs build-commands insert-all insert-commands

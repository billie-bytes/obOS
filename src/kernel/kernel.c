#include <stdint.h>
#include "kernel/gdt.h"
#include "kernel/kernel-entrypoint.h"
#include "kernel/framebuffer.h"
#include "kernel/interrupt.h"
#include "kernel/idt.h"
#include "kernel/keyboard.h"
#include "kernel/disk.h"
#include "kernel/ext2.h"
#include "kernel/paging.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/speaker.h"
#include <stdbool.h>

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_ext2();
    gdt_install_tss();
    set_tss_register();
    speaker_init();
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);


    char name[] = "shell";
    char* argv[] = {name};
    struct EXT2ProgramRequest init_process = {
        .buf         = (uint8_t*) 0,
        .name        = name,
        .name_len    = 5,
        .flags       = 3, // 0b00000011
        .parent_inode= 2, //Root directory inode
        .buffer_size = 0x100000,
        .argc = 1,
        .argv = argv
    };
    set_tss_kernel_current_stack();
    process_create_user_process(init_process);
    scheduler_init();
    scheduler_switch_to_next_process();

}




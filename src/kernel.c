#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"
#include "header/driver/keyboard.h"
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"
#include "header/memory/paging.h"
#include "header/process/process.h"
#include "header/scheduler/scheduler.h"
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

    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);
    // Write shell into memory
    struct EXT2DriverRequest request = {
        .buf         = (uint8_t*) 0,
        .name        = "shell",
        .name_len    = 5,
        .parent_inode= 2,        // root directory inode (adjust if book expects 1)
        .buffer_size = 0x100000,
        .is_folder   = false,
    };

    // Set TSS.esp0 for interprivilege interrupt
    set_tss_kernel_current_stack();

    // Create init process and execute it
    process_create_user_process(request);
    scheduler_init();
    scheduler_switch_to_next_process();

}




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
#include <stdbool.h>

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    asm volatile("sti");
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    int row = 0, col = 0;
    keyboard_state_activate();
    
    /*FILESYSTEM INITIALIZATION*/
    paging_init_page_manager_state();
    initialize_filesystem_ext2();
    // create_ext2();
    /* TSS INITIALIZATION FOR USER MODE */
    gdt_install_tss();
    set_tss_register();

    // Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);

    // Write shell into memory
    struct EXT2DriverRequest request = {
        .buf                          = (uint8_t*) 0,
        .name                         = "shell",
        .parent_inode                 = 1,
        .buffer_size                  = 0x100000,
        .name_len                     = 5,
    };
    read(request);

    // Set TSS $esp pointer and jump into shell 
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t*) 0);

    while (true) {
        char c;
        get_keyboard_buffer(&c);
        if (c) {
            if (c == '\b') {
                if (col > 0) {
                    col--;
                    framebuffer_write(row, col, ' ', 0x0F, 0x00);
                } else if (row > 0) {
                    row--;
                    col = 79;
                    framebuffer_write(row, col, ' ', 0x0F, 0x00);
                }
            }
            else if (c == '\n'){
                col = 0;
                row++;
            }
            else {
                framebuffer_write(row, col, c, 0xF, 0);
                if (col >= 80) {
                    ++row;
                    col = 0;
                } else {
                    ++col;
                }
            }
            framebuffer_set_cursor(row, col);
        }
    }

}




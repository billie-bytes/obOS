#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"
#include "header/driver/keyboard.h"
#include "header/driver/disk.h"
#include <stdbool.h>

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    int row = 0, col = 0;
    keyboard_state_activate();

    struct BlockBuffer b;
    for (int i = 0; i < 512; i++) b.buf[i] = i % 16;
    write_blocks(&b, 17, 1);

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




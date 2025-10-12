#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    uint16_t indexAttribute = ((bg << 4) | (fg & 0x0F));
    uint16_t *where;
    uint16_t index = (col * 80) + row;
    where = &FRAMEBUFFER_MEMORY_OFFSET[index];
    *where = (c | (attribute << 8));
}

// void framebuffer_set_cursor(uint8_t r, uint8_t c) {
//     //
// }

void framebuffer_clear(void) {
    uint16_t *textmemptr = FRAMEBUFFER_MEMORY_OFFSET;
    uint16_t blank = ((0x20) | (0x0F << 8));
    for (int i = 0; i < 25; i++) {
        memset (textmemptr + i * 80, blank, 80);
    }
}

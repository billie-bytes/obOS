#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    uint16_t *vmem = (uint16_t*)FRAMEBUFFER_MEMORY_OFFSET; 
    uint16_t indexAttribute = ((bg << 4) | (fg & 0x0F));
    uint16_t index = (row * 80) + col;
    vmem[index] = ((uint16_t)c) | (indexAttribute << 8);
}

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    uint16_t pos = r * 80 + c;

	out(0x3D4, 0x0F);
	out(0x3D5, (uint8_t) (pos & 0xFF));
	out(0x3D4, 0x0E);
	out(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_clear(void) {
    uint16_t *textmemptr = (uint16_t*)FRAMEBUFFER_MEMORY_OFFSET;
    uint16_t blank = ((uint16_t)' ') | ((uint16_t)0x0F << 8);
    for (int i = 0; i < 80*80; i++) {
        textmemptr[i] = blank;
        framebuffer_set_cursor(0,0);
    }
}


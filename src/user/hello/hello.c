#include <stdint.h>
#include "lib/string.h"

#define COLOR_TEXT 0x0F

static inline void syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("int $0x30" : : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

static inline void sys_puts(const char *s, uint32_t len, uint8_t color) {
    syscall_do(6, (uint32_t)s, len, (uint32_t)color);
}

static inline void sys_putchar(char c, uint8_t color) {
    syscall_do(5, (uint32_t)(uint8_t)c, (uint32_t)color, 0);
}

int main(void) {
    sys_puts("Hello, World!", 13, COLOR_TEXT);
    sys_putchar('\n', COLOR_TEXT);
    return 0;
}

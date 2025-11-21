#include <stdint.h>

#define COLOR_PROMPT_USER  0x0A  /* light green */
#define COLOR_PROMPT_SEP   0x07  /* gray */
#define COLOR_INPUT        0x0F  /* white */
#define MAX_INPUT_LEN 128 /* hanya untuk batas backspace */

static inline void syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

static inline void sys_putchar(char c, uint8_t color) {
    syscall_do(5u, (uint32_t)(uint8_t)c, (uint32_t)color, 0u);
}
static inline void sys_puts(const char *s, uint32_t len, uint8_t color) {
    syscall_do(6u, (uint32_t)s, len, (uint32_t)color);
}
static inline void sys_getchar(char *out) {
    syscall_do(4u, (uint32_t)out, 0, 0);
}
static inline void sys_keyboard_activate(void) {
    syscall_do(7u, 0, 0, 0);
}

static void print_prompt(void) {
    sys_puts("jOSh@OS-IF2230", 15, COLOR_PROMPT_USER);
    sys_puts(":", 1, COLOR_PROMPT_SEP);
    sys_putchar(' ', COLOR_PROMPT_SEP);
}

int main(void) {
    uint32_t len = 0; /* jumlah karakter pada baris saat ini */

    sys_keyboard_activate();
    print_prompt();

    while (1) {
        char c = 0;
        sys_getchar(&c);
        if (!c) continue; /* no new key */

        if (c == '\b') { /* backspace */
            if (len > 0) {
                len--;
                sys_putchar('\b', COLOR_INPUT);
                sys_putchar(' ', COLOR_INPUT);
                sys_putchar('\b', COLOR_INPUT);
            }
            continue;
        }
        if (c == '\n') { /* enter */
            sys_putchar('\n', COLOR_INPUT);
            len = 0;
            print_prompt();
            continue;
        }
        if (c == 3) { /* Ctrl+C */
            sys_puts("^C\n", 3, COLOR_INPUT);
            len = 0;
            print_prompt();
            continue;
        }

        if (len < MAX_INPUT_LEN) {
            sys_putchar(c, COLOR_INPUT);
            len++;
        }
    }
    return 0;
}

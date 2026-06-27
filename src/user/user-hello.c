#include <stdint.h>
#include "lib/string.h"
#include "user/command/syscall.h"

#define COLOR_TEXT 0x0F
#define COLOR_PROMPT_USER  0x0A
#define COLOR_PROMPT_SEP   0x07

char current_path[256] = "/";

static void print_prompt(void) {
    sys_puts("jOSh@OS-IF2230", 14, COLOR_PROMPT_USER);
    sys_puts(":", 1, COLOR_PROMPT_SEP);
    sys_puts(current_path, strlen(current_path), COLOR_PROMPT_USER);
    sys_puts("$ ", 2, COLOR_PROMPT_SEP);
}

int main(void) {
    sys_puts("Hello, World!", 13, COLOR_TEXT);
    sys_putchar('\n', COLOR_TEXT);
    print_prompt();
    sys_exit();
    while(1);  // Should never reach here
    return 0;
}

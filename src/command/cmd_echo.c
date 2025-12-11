#include "syscall.h"
#include "../header/stdlib/string.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_putchar('\n', COLOR_TXT);
        return 0;
    }
    
    // Print all arguments separated by space
    for (int i = 1; i < argc; i++) {
        sys_puts(argv[i], strlen(argv[i]), COLOR_TXT);
        if (i < argc - 1) {
            sys_putchar(' ', COLOR_TXT);
        }
    }
    sys_putchar('\n', COLOR_TXT);
    
    return 0;
}

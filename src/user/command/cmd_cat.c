#include "user/command/syscall.h"
#include "lib/string.h"

#define MAX_LINE 256
#define DIRBUF_BYTES (BLOCK_SIZE * 8)

int main(int argc, char* argv[]) {
    if (argc < 2) { 
        sys_puts("cat: no argument\n", 17, COLOR_TXT); 
        return 1; 
    }
    if (argc > 3) { 
        sys_puts("cat: too many arguments\n", 24, COLOR_TXT); 
        return 1; 
    }
    
    const char* path = argv[1];
    uint8_t type = 0;
    
    uint32_t target_inode = sys_stat(path, &type);

    if (target_inode == 0) {
        sys_puts("cat: no such file or directory\n", 31, COLOR_TXT);
        return 1;
    }
    
    if (type == EXT2_FT_DIR) {
        sys_puts("cat: not a file\n", 16, COLOR_TXT);
        return 1;
    }
    
    uint8_t filebuf[4096];
    int32_t bytes_read = sys_read(target_inode, filebuf, sizeof(filebuf));
    
    if (bytes_read >= 0) {
        for (int32_t i = 0; i < bytes_read; i++) {
            char c = (char)filebuf[i];
            if (c == 0) break;
            sys_putchar(c, COLOR_TXT);
        }
        sys_putchar('\n', COLOR_TXT);
    } else {
        sys_puts("cat: error code \n", 17, COLOR_TXT);
        return 1;
    }
    
    return 0;
}
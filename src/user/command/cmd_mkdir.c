#include "user/command/syscall.h"
#include "lib/string.h"

#define MAX_LINE 256

// We keep split_path because mkdir needs to separate the target directory name 
// from its parent path to correctly populate the EXT2DriverRequest.
static void split_path(const char* in, char* parent_out, char* base_out) {
    if (!in || !*in) { 
        strcpy(parent_out, "."); 
        strcpy(base_out, ""); 
        return; 
    }
    
    char tmp[MAX_LINE]; 
    strncpy(tmp, in, sizeof(tmp)); 
    tmp[sizeof(tmp)-1] = 0;
    
    size_t L = strlen(tmp);
    while (L > 1 && tmp[L-1] == '/') { tmp[--L] = 0; }
    
    char* last = strrchr(tmp, '/');
    if (!last) { 
        strcpy(parent_out, "."); 
        strcpy(base_out, tmp); 
        return; 
    }
    if (last == tmp) { 
        strcpy(parent_out, "/"); 
        strcpy(base_out, tmp+1); 
        return; 
    }
    
    *last = 0; 
    strcpy(parent_out, tmp); 
    strcpy(base_out, last+1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { 
        sys_puts("mkdir: missing operand\n", 23, COLOR_TXT); 
        return 1; 
    }
    
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);
    
    if (base[0] == 0) { 
        sys_puts("mkdir: invalid name\n", 20, COLOR_TXT); 
        return 1; 
    }

    if (sys_stat(argv[1], NULL) != 0) {
        sys_puts("mkdir: file or directory already exists\n", 40, COLOR_TXT);
        return 1;
    }
    
    uint32_t parent_inode = sys_stat(parent_path, NULL);
    
    if (parent_inode == 0) { 
        sys_puts("mkdir: parent directory not found\n", 34, COLOR_TXT); 
        return 1; 
    }
    
    struct EXT2DriverRequest req = {
        .buf = 0,
        .name = base,
        .name_len = (uint8_t)strlen(base),
        .parent_inode = parent_inode,
        .buffer_size = 0,
        .is_folder = true
    };
    
    int8_t rc = sys_write(&req);
    if (rc == 0) {
        sys_puts("mkdir: success\n", 15, COLOR_TXT);
        return 0;
    }
    
    sys_puts("mkdir: error code ", 18, COLOR_TXT);
    sys_putchar('0' + rc, COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
    return 1;
}
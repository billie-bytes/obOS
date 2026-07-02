#include "user/command/syscall.h"
#include "lib/string.h"

#define MAX_LINE 256

static void split_path(const char* in, char* parent_out, char* base_out) {
    if (!in || !*in) { strcpy(parent_out, "."); strcpy(base_out, ""); return; }
    char tmp[MAX_LINE]; strncpy(tmp, in, sizeof(tmp)); tmp[sizeof(tmp)-1] = 0;
    size_t L = strlen(tmp);
    while (L > 1 && tmp[L-1] == '/') { tmp[--L] = 0; }
    char* last = strrchr(tmp, '/');
    if (!last) { strcpy(parent_out, "."); strcpy(base_out, tmp); return; }
    if (last == tmp) { strcpy(parent_out, "/"); strcpy(base_out, tmp+1); return; }
    *last = 0; strcpy(parent_out, tmp); strcpy(base_out, last+1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_puts("exec: missing program name\n", 28, COLOR_TXT);
        return 1;
    }
    
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);
    
    uint32_t parent_inode = sys_stat(parent_path, NULL);
    
    if (parent_inode == 0) {
        sys_puts("exec: parent directory not found\n", 33, COLOR_TXT);
        return 1;
    }
    
    uint32_t process_buffer = (2 * 1024 * 1024);
    struct EXT2ProgramRequest req = {
        .buf          = (uint8_t*)0,
        .name         = base,
        .name_len     = (uint8_t)strlen(base),
        .flags        = (FOREGROUND_PROCESS|TAKES_INPUT),
        .parent_inode = parent_inode,
        .buffer_size  = process_buffer,
        .argc         = argc - 1,
        .argv         = &argv[1]
    };
    
    int32_t res = sys_exec(&req);
    if (res < 0) {
        sys_puts("exec: failed to execute\n", 24, COLOR_TXT);
        return 1;
    }
    
    return 0;
}
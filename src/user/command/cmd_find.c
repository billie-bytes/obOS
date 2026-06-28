#include "user/command/syscall.h"
#include "lib/string.h"

#define DIRBUF_BYTES (BLOCK_SIZE * 8)

struct DirectoryTraversal {
    uint8_t* base;
    uint32_t size;
    uint32_t off;
};

static bool dirwalk_next(struct DirectoryTraversal* it, struct EXT2DirectoryEntry* out_entry, char* name_buf, uint16_t name_buf_sz) {
    if (it->off >= it->size) return false;
    uint8_t* p = it->base + it->off;
    uint32_t inode = *(uint32_t*)(p + 0);
    uint16_t rec = *(uint16_t*)(p + 4);
    uint16_t nlen = *(uint16_t*)(p + 6);
    uint8_t ftype = *(uint8_t*)(p + 8);
    if (rec == 0 || inode == 0) return false;
    if (it->off + rec > it->size) return false;
    out_entry->inode = inode;
    out_entry->rec_len = rec;
    out_entry->name_len = nlen;
    out_entry->file_type = ftype;
    uint8_t copy_len = nlen;
    if (copy_len >= name_buf_sz) copy_len = name_buf_sz - 1;
    for (uint8_t i = 0; i < copy_len; i++) name_buf[i] = p[9 + i];
    name_buf[copy_len] = '\0';
    it->off += rec;
    return true;
}

static void find_recursive(uint32_t current_inode, const char* current_path) {
    uint8_t dirbuf[4096]; 
    
    if (sys_readdir(current_inode, dirbuf) < 0) {
        return;
    }

    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry entry;
    char name[256];

    while (dirwalk_next(&it, &entry, name, sizeof(name))) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        char full_path[512];
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path)-1] != '/') strcat(full_path, "/");
        strcat(full_path, name);

        sys_puts(full_path, (uint32_t)strlen(full_path), COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);

        if (entry.file_type == EXT2_FT_DIR) {
            find_recursive(entry.inode, full_path);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) { 
        sys_puts("find: missing path\n", 19, COLOR_TXT); 
        return 1; 
    }
    
    uint8_t type = 0;
    
    // Kernel handles all relative (.), absolute (/), and nested paths natively
    uint32_t target_inode = sys_stat(argv[1], &type);
    
    if (target_inode == 0) {
        sys_puts("find: '", 7, COLOR_TXT);
        sys_puts(argv[1], strlen(argv[1]), COLOR_TXT);
        sys_puts("': No such file or directory\n", 29, COLOR_TXT);
        return 1; 
    }

    // Print the root of the search
    sys_puts(argv[1], (uint32_t)strlen(argv[1]), COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
    
    if (type == EXT2_FT_DIR) {
        find_recursive(target_inode, argv[1]);
    }
    
    return 0;
}
#include "user/command/syscall.h"
#include "lib/string.h"

#define DIRBUF_BYTES (BLOCK_SIZE * 8)
#define COLOR_DIR 0x0B

struct DirectoryTraversal {
    uint8_t* base;
    uint32_t size;
    uint32_t off;
};

volatile static bool dirwalk_next(struct DirectoryTraversal* it, struct EXT2DirectoryEntry* out_entry, char* name_buf, uint16_t name_buf_sz) {
    if (it->off >= it->size) {
        sys_puts("Invalid offset\n",15,COLOR_TXT);
        return false;
    }
    
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

int main(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : ".";
    
    if (argc > 2) {
        sys_puts("ls: too many arguments\n", 24, COLOR_TXT);
        return 1;
    }

    uint8_t type = 0;
    uint32_t target_inode = sys_stat(path, &type);
    
    if (target_inode == 0) {
        sys_puts("ls: no such file or directory\n", 31, COLOR_TXT);
        return 1;
    }
    
    if (type != EXT2_FT_DIR) {
        sys_puts("ls: not a folder\n", 18, COLOR_TXT);
        return 1;
    }

    uint8_t dirbuf[DIRBUF_BYTES];
    int32_t rc = sys_readdir(target_inode, dirbuf,DIRBUF_BYTES);
    
    if (rc < 0) {
        sys_puts("ls: error reading directory\n", 28, COLOR_TXT);
        return 1;
    }
    
    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry e;
    char nm[256];
    while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
        if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
        
        uint8_t color = (e.file_type == EXT2_FT_DIR) ? COLOR_DIR : COLOR_TXT;
        sys_puts(nm, strlen(nm), color);
        sys_putchar('\n', COLOR_TXT);
    }
    return 0;
}
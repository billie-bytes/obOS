#include "syscall.h"
#include "../header/stdlib/string.h"

#define DIRBUF_BYTES (BLOCK_SIZE * 8)
#define COLOR_DIR 0x0B

// Get current working directory from syscall 22
static inline uint32_t get_cwd_inode(void) {
    uint32_t retval;
    char buf[256];
    syscall_do(22, (uint32_t)buf, sizeof(buf), 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

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

static bool resolve_path(const char *path, uint32_t *out_inode) {
    if (path[0] == '/' && path[1] == '\0') { *out_inode = 2; return true; }
    uint32_t inode = (path[0] == '/') ? 2 : get_cwd_inode();
    char buf[256]; strncpy(buf, path, 255); buf[255] = 0;
    char* token = buf; if (token[0] == '/') token++;
    while (*token) {
        char* end = token; while (*end && *end != '/') end++;
        bool last = (*end == '\0'); if (!last) *end = '\0';
        if (strcmp(token, ".") == 0) { token = last ? end : end + 1; continue; }
        if (strcmp(token, "..") == 0) {
            uint8_t pbuf[BLOCK_SIZE];
            struct EXT2DriverRequest req = { .buf = pbuf, .name = "..", .name_len = 2, .parent_inode = inode, .buffer_size = sizeof(pbuf), .is_folder = true };
            int8_t rc = sys_readdir(&req); if (rc != 0) return false;
            struct DirectoryTraversal it = { .base = pbuf, .size = sizeof(pbuf), .off = 0 };
            struct EXT2DirectoryEntry e; char nm[256];
            if (dirwalk_next(&it, &e, nm, sizeof(nm)) && strcmp(nm, "..") == 0) inode = e.inode;
            token = last ? end : end + 1; continue;
        }
        uint8_t dbuf[DIRBUF_BYTES];
        struct EXT2DriverRequest req = { .buf = dbuf, .name = ".", .name_len = 1, .parent_inode = inode, .buffer_size = sizeof(dbuf), .is_folder = true };
        int8_t rc = sys_readdir(&req); if (rc != 0) return false;
        struct DirectoryTraversal it = { .base = dbuf, .size = sizeof(dbuf), .off = 0 };
        struct EXT2DirectoryEntry e; char nm[256]; bool found = false;
        while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
            if (strcmp(nm, token) == 0) { inode = e.inode; found = true; break; }
        }
        if (!found) return false;
        token = last ? end : end + 1;
    }
    *out_inode = inode; return true;
}

int main(int argc, char* argv[]) {
    uint32_t target = get_cwd_inode();
    if (argc > 2) { sys_puts("ls: too many arguments\n", 24, COLOR_TXT); return 1; }
    if (argc == 2) {
        if (!resolve_path(argv[1], &target)) { 
            sys_puts("ls: no such file or directory\n", 31, COLOR_TXT); 
            return 1; 
        }
    }
    
    uint8_t dirbuf[DIRBUF_BYTES];
    struct EXT2DriverRequest req = {
        .buf = dirbuf,
        .name = ".",
        .name_len = 1,
        .parent_inode = target,
        .buffer_size = sizeof(dirbuf),
        .is_folder = true
    };
    int8_t rc = sys_readdir(&req);
    if (rc != 0) { 
        sys_puts("ls: error code ", 15, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
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

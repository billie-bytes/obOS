#include "syscall.h"
#include "../header/stdlib/string.h"

#define MAX_LINE 256
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

static uint32_t find_child(uint32_t dir_inode, const char* name) {
    uint8_t dirbuf[DIRBUF_BYTES];
    struct EXT2DriverRequest req = {.buf = dirbuf, .name = ".", .name_len = 1, .parent_inode = dir_inode, .buffer_size = sizeof(dirbuf), .is_folder = true};
    int8_t rc = sys_readdir(&req);
    if (rc != 0) return 0;
    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry e; char nm[256];
    while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
        if (strcmp(nm, name) == 0) return e.inode;
    }
    return 0;
}

static bool resolve_path(const char* path, uint32_t* out_inode) {
    if (!path || !*path) { *out_inode = sys_getcwd(NULL, 0); return true; }
    if (strcmp(path, "/") == 0) { *out_inode = 2; return true; }
    uint32_t cur = (path[0] == '/') ? 2 : sys_getcwd(NULL, 0);
    char tmp[MAX_LINE]; strncpy(tmp, path, sizeof(tmp)); tmp[sizeof(tmp)-1] = 0;
    char* s = tmp; if (*s == '/') s++;
    for (char* tok = strtok(s, "/"); tok; tok = strtok(NULL, "/")) {
        if (strcmp(tok, ".") == 0) continue;
        if (strcmp(tok, "..") == 0) { uint32_t up = find_child(cur, ".."); if (!up) return false; cur = up; continue; }
        uint32_t nxt = find_child(cur, tok);
        if (!nxt) return false;
        cur = nxt;
    }
    *out_inode = cur; return true;
}

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
        sys_puts("mkdir: missing operand\n", 24, COLOR_TXT); 
        return 1; 
    }
    
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);
    if (base[0] == 0) { sys_puts("mkdir: invalid name\n", 21, COLOR_TXT); return 1; }
    
    uint32_t parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("mkdir: parent not found\n", 25, COLOR_TXT); return 1; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = sys_getcwd(NULL, 0);
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("mkdir: parent not found\n", 25, COLOR_TXT); return 1; }
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

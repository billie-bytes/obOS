#include "user/command/syscall.h"
#include "lib/string.h"

#define MAX_LINE 256
#define DIRBUF_BYTES (BLOCK_SIZE * 8)
#define EXT2_FT_DIR 2

static inline uint32_t sys_getcwd_inode(void) {
    uint32_t retval;
    char buf[256];
    syscall_do(22, (uint32_t)buf, sizeof(buf), 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

extern int strcmp(const char *s1, const char *s2);
extern size_t strlen(const char *str);
extern char* strcpy(char *dest, const char *src);
extern char* strncpy(char *dest, const char *src, size_t n);
extern char* strcat(char *dest, const char *src);
extern char* strrchr(const char *s, int c);
extern char* strtok(char *s, const char *delim);

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
    if (!path || !*path) { *out_inode = sys_getcwd_inode(); return true; }
    if (strcmp(path, "/") == 0) { *out_inode = 2; return true; }
    uint32_t cur = (path[0] == '/') ? 2 : sys_getcwd_inode();
    char tmp[MAX_LINE];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = 0;
    char* s = tmp;
    if (*s == '/') s++;
    for (char* tok = strtok(s, "/"); tok; tok = strtok(0, "/")) {
        if (strcmp(tok, ".") == 0) continue;
        if (strcmp(tok, "..") == 0) {
            uint32_t up = find_child(cur, "..");
            if (!up) return false;
            cur = up;
            continue;
        }
        uint32_t nxt = find_child(cur, tok);
        if (!nxt) return false;
        cur = nxt;
    }
    *out_inode = cur;
    return true;
}

static void find_recursive(uint32_t current_inode, const char* current_path) {
    uint8_t dirbuf[4096]; 
    int8_t rc = 0;

    struct EXT2DriverRequest req = {
        .buf          = dirbuf,
        .name         = ".",
        .name_len     = 1,
        .parent_inode = current_inode,
        .buffer_size  = sizeof(dirbuf),
        .is_folder    = true
    };

    rc = sys_readdir(&req);
    if (rc != 0) {
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
    
    uint32_t parent_inode = 0;
    if (argv[1][0] == '/') {
        if (strcmp(argv[1], "/") == 0) parent_inode = 2;
        else if (!resolve_path(argv[1], &parent_inode)) { 
            sys_puts("find: '", 7, COLOR_TXT);
            sys_puts(argv[1], strlen(argv[1]), COLOR_TXT);
            sys_puts("': No such file or directory\n", 29, COLOR_TXT);
            return 1; 
        }
    } else {
        if (strcmp(argv[1], ".") == 0) parent_inode = sys_getcwd_inode();
        else if (!resolve_path(argv[1], &parent_inode)) { 
            sys_puts("find: '", 7, COLOR_TXT);
            sys_puts(argv[1], strlen(argv[1]), COLOR_TXT);
            sys_puts("': No such file or directory\n", 29, COLOR_TXT);
            return 1; 
        }
    }

    uint8_t dirbuf[1024]; 
    int8_t rc = 0;
    struct EXT2DriverRequest req = {
        .buf          = dirbuf,
        .name         = ".",
        .name_len     = 1,
        .parent_inode = parent_inode,
        .buffer_size  = sizeof(dirbuf),
        .is_folder    = true
    };
    rc = sys_readdir(&req);

    if (rc != 0) {
        sys_puts(argv[1], (uint32_t)strlen(argv[1]), COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
    } else {
        sys_puts(argv[1], (uint32_t)strlen(argv[1]), COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
        find_recursive(parent_inode, argv[1]);
    }
    
    return 0;
}

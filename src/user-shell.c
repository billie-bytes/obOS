#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"

#define COLOR_PROMPT_USER  0x0A
#define COLOR_PROMPT_SEP   0x07
#define COLOR_INPUT        0x0F
#define COLOR_TXT          0x07
#define COLOR_DIR          0x09
#define MAX_INPUT_LEN 256
#define MAX_ARGS 16
#define MAX_LINE 256
#define BLOCK_SIZE 4096
#define BLOCK_COUNT 16
#define DIRBUF_BYTES (BLOCK_COUNT * BLOCK_SIZE)
#define EXT2_FT_DIR 2
#define EXT2_FT_REG_FILE 1
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFREG 0x8000

struct EXT2DirectoryEntry {
    uint32_t inode;
    uint16_t rec_len;
    uint16_t name_len;
    uint8_t file_type;
} __attribute__((packed));

struct EXT2DriverRequest {
    void *buf;
    char *name;
    uint8_t name_len;
    uint32_t parent_inode;
    uint32_t buffer_size;
    bool is_folder;
} __attribute__((packed));

uint32_t current_directory_inode = 2;
char current_path[256] = "/";

void syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

static inline void sys_putchar(char c, uint8_t color) {
    syscall_do(5u, (uint32_t)(uint8_t)c, (uint32_t)color, 0u);
}

static inline void sys_puts(const char *s, uint32_t len, uint8_t color) {
    syscall_do(6u, (uint32_t)s, len, (uint32_t)color);
}

static inline void sys_getchar(char *out) {
    syscall_do(4u, (uint32_t)out, 0, 0);
}

static inline void sys_keyboard_activate(void) {
    syscall_do(7u, 0, 0, 0);
}

static inline void putc_color(char c, uint8_t fg) {
    syscall_do(5u, (uint32_t)(uint8_t)c, (uint32_t)fg, 0u);
}

static inline void puts_color(const char* s, uint8_t fg, uint32_t n) {
    syscall_do(6u, (uint32_t)s, n, (uint32_t)fg);
}

static inline int8_t fs_readfile(struct EXT2DriverRequest* r, int8_t* rc) {
    syscall_do(0, (uint32_t)r, (uint32_t)rc, 0);
    return *rc;
}

static inline int8_t fs_readdir(struct EXT2DriverRequest* r, int8_t* rc) {
    syscall_do(1, (uint32_t)r, (uint32_t)rc, 0);
    return *rc;
}

static inline int8_t fs_write(struct EXT2DriverRequest* r, int8_t* rc) {
    syscall_do(2, (uint32_t)r, (uint32_t)rc, 0);
    return *rc;
}

static inline int8_t fs_delete(struct EXT2DriverRequest* r, int8_t* rc) {
    syscall_do(3, (uint32_t)r, (uint32_t)rc, 0);
    return *rc;
}

extern size_t strlen(const char *str);
// size_t strlen(const char *str) {
//     size_t len = 0;
//     while (str[len] != '\0') len++;
//     return len;
// }

static void print_prompt(void) {
    sys_puts("jOSh@OS-IF2230", 14, COLOR_PROMPT_USER);
    sys_puts(":", 1, COLOR_PROMPT_SEP);
    sys_puts(current_path, strlen(current_path), COLOR_PROMPT_USER);
    sys_puts("$ ", 2, COLOR_PROMPT_SEP);
}


// int strcmp(const char *s1, const char *s2) {
//     while (*s1 && *s2 && *s1 == *s2) {
//         s1++;
//         s2++;
//     }
//     return (unsigned char)*s1 - (unsigned char)*s2;
// }
extern int strcmp(const char *s1, const char *s2);

// int strncmp(const char *s1, const char *s2, size_t n) {
//     for (size_t i = 0; i < n; i++) {
//         if (!s1[i] || !s2[i] || s1[i] != s2[i])
//             return (unsigned char)s1[i] - (unsigned char)s2[i];
//     }
//     return 0;
// }
extern int strncmp(const char *s1, const char *s2, size_t n);

// char* strcpy(char *dest, const char *src) {
//     char *p = dest;
//     while ((*p++ = *src++) != '\0');
//     return dest;
// }
extern char* strcpy(char *dest, const char *src);

// char* strncpy(char *dest, const char *src, size_t n) {
//     size_t i = 0;
//     while (i < n && src[i] != '\0') {
//         dest[i] = src[i];
//         i++;
//     }
//     while (i < n) {
//         dest[i] = '\0';
//         i++;
//     }
//     return dest;
// }
extern char* strncpy(char *dest, const char *src, size_t n);

// char* strcat(char *dest, const char *src) {
//     char *p = dest;
//     while (*p) p++;
//     while ((*p++ = *src++) != '\0');
//     return dest;
// }
extern char* strcat(char *dest, const char *src);

// char* strncat(char *dest, const char *src, size_t n) {
//     char *p = dest;
//     while (*p) p++;
//     for (size_t i = 0; i < n && src[i]; i++)
//         *p++ = src[i];
//     *p = '\0';
//     return dest;
// }
extern char* strncat(char *dest, const char *src, size_t n);

// char* strchr(const char *s, int c) {
//     while (*s) {
//         if (*s == (char)c) return (char*)s;
//         s++;
//     }
//     if ((char)c == '\0') return (char*)s;
//     return NULL;
// }
extern char* strchr(const char *s, int c);

// char* strrchr(const char *s, int c) {
//     const char *last = NULL;
//     while (*s) {
//         if (*s == (char)c) last = s;
//         s++;
//     }
//     if ((char)c == '\0') return (char*)s;
//     return (char*)last;
// }
extern char* strrchr(const char *s, int c);

// char* strtok(char *s, const char *delim) {
//     static char *next = NULL;
//     if (s) next = s;
//     if (!next) return NULL;
//     while (*next && strchr(delim, *next)) next++;
//     if (!*next) return NULL;
//     char *start = next;
//     while (*next && !strchr(delim, *next)) next++;
//     if (*next) *next++ = '\0';
//     return start;
// }
extern char* strtok(char *s, const char *delim);

// void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
//     uint8_t *dstbuf       = (uint8_t*) dest;
//     const uint8_t *srcbuf = (const uint8_t*) src;
//     for (size_t i = 0; i < n; i++)
//         dstbuf[i] = srcbuf[i];
//     return dstbuf;
// }
extern void* memcpy(void* dest, const void* src, size_t n);

struct DirectoryTraversal {
    uint8_t* base;
    uint32_t size;
    uint32_t off;
};

static bool dirwalk_next(struct DirectoryTraversal* it, struct EXT2DirectoryEntry* out_entry, char* name_buf, uint16_t name_buf_sz) {
    while (it->off + 9 <= it->size) {
        uint8_t* p = it->base + it->off;
        uint32_t inode  = *(uint32_t*)(p + 0);
        uint16_t rec    = *(uint16_t*)(p + 4);
        uint16_t nlen   = *(uint16_t*)(p + 6);
        uint8_t  ftype  = *(uint8_t *)(p + 8);

        if (rec == 0) return false;
        if (it->off + rec > it->size) return false;
        it->off += rec;
        if (inode == 0 || nlen == 0) continue;
        if (9u + nlen > rec) continue;
        if (nlen >= name_buf_sz) nlen = (uint16_t)(name_buf_sz - 1);
        memcpy(name_buf, p + 9, nlen);
        name_buf[nlen] = 0;
        out_entry->inode     = inode;
        out_entry->rec_len   = rec;
        out_entry->name_len  = nlen;
        out_entry->file_type = ftype;
        return true;
    }
    return false;
}

static int8_t fetch_dir_table(uint32_t dir_inode, uint8_t* buf, uint32_t buf_sz) {
    struct EXT2DriverRequest req = {
        .buf          = buf,
        .name         = ".",
        .name_len     = 1,
        .parent_inode = dir_inode,
        .buffer_size  = buf_sz,
        .is_folder    = true
    };
    int8_t rc = -1;
    return fs_readdir(&req, &rc);
}

static uint32_t find_child(uint32_t dir_inode, const char* name, uint8_t* out_type) {
    uint8_t dirbuf[DIRBUF_BYTES];
    if (fetch_dir_table(dir_inode, dirbuf, sizeof(dirbuf)) != 0) return 0;
    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry e;
    char nm[256];
    while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
        if (strcmp(nm, name) == 0) {
            if (out_type) *out_type = e.file_type;
            return e.inode;
        }
    }
    return 0;
}

static bool resolve_path(const char* path, uint32_t* out_inode) {
    if (!path || !*path) { *out_inode = current_directory_inode; return true; }
    if (strcmp(path, "/") == 0) { *out_inode = 2; return true; }
    uint32_t cur = (path[0] == '/') ? 2 : current_directory_inode;
    char tmp[MAX_LINE];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = 0;
    char* s = tmp;
    if (*s == '/') s++;
    for (char* tok = strtok(s, "/"); tok; tok = strtok(NULL, "/")) {
        if (strcmp(tok, ".") == 0) continue;
        if (strcmp(tok, "..") == 0) {
            uint32_t up = find_child(cur, "..", NULL);
            if (!up) return false;
            cur = up;
            continue;
        }
        uint8_t t = 0;
        uint32_t nxt = find_child(cur, tok, &t);
        if (!nxt) return false;
        cur = nxt;
    }
    *out_inode = cur;
    return true;
}

static void split_path(const char* in, char* parent_out, char* base_out) {
    if (!in || !*in) { strcpy(parent_out, "."); strcpy(base_out, ""); return; }
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

static void cmd_pwd(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_puts(current_path, strlen(current_path), COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
}

static void cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_puts("Commands:\n", 10, COLOR_TXT);
    sys_puts("  ls [PATH]\n", 12, COLOR_TXT);
    sys_puts("  cd [PATH]\n", 12, COLOR_TXT);
    sys_puts("  pwd\n", 6, COLOR_TXT);
    sys_puts("  cat <FILE>\n", 13, COLOR_TXT);
    sys_puts("  mkdir <DIR>\n", 14, COLOR_TXT);
    sys_puts("  rm <FILE|DIR>\n", 15, COLOR_TXT);
    sys_puts("  cp <SRC> <DST>\n", 16, COLOR_TXT);
    sys_puts("  mv <SRC> <DST>\n", 16, COLOR_TXT);
    sys_puts("  find <NAME>\n", 14, COLOR_TXT);
    sys_puts("  grep <PATTERN> <FILE>\n", 24, COLOR_TXT);
    sys_puts("  clear\n", 8, COLOR_TXT);
    sys_puts("  help\n", 7, COLOR_TXT);
    sys_puts("  exit\n", 7, COLOR_TXT);
}

static void cmd_ls(int argc, char* argv[]) {
    uint32_t target = current_directory_inode;
    if (argc > 2) { sys_puts("ls: too many arguments\n", 24, COLOR_TXT); return; }
    if (argc == 2) {
        if (!resolve_path(argv[1], &target)) { sys_puts("ls: no such file or directory\n", 31, COLOR_TXT); return; }
    }
    uint8_t dirbuf[DIRBUF_BYTES];
    int8_t rc = -1;
    struct EXT2DriverRequest req = {
        .buf          = dirbuf,
        .name         = ".",
        .name_len     = 1,
        .parent_inode = target,
        .buffer_size  = sizeof(dirbuf),
        .is_folder    = true
    };
    rc = fs_readdir(&req, &rc);
    if (rc != 0) { 
        sys_puts("ls: error code ", 15, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
        return; 
    }
    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry e;
    char nm[256];
    while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
        if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
        uint8_t color = (e.file_type == EXT2_FT_DIR) ? COLOR_DIR : COLOR_TXT;
        puts_color(nm, color, (uint32_t)strlen(nm));
        sys_putchar('\n', COLOR_TXT);
    }
}

static void cmd_cd(int argc, char* argv[]) {
    if (argc < 2) { current_directory_inode = 2; strcpy(current_path, "/"); return; }
    uint32_t target;
    if (!resolve_path(argv[1], &target)) { sys_puts("cd: no such file or directory\n", 31, COLOR_TXT); return; }
    uint8_t dummy[BLOCK_SIZE];
    int8_t rc = -1;
    struct EXT2DriverRequest req = {
        .buf          = dummy,
        .name         = ".",
        .name_len     = 1,
        .parent_inode = target,
        .buffer_size  = sizeof(dummy),
        .is_folder    = true
    };
    rc = fs_readdir(&req, &rc);
    if (rc != 0) { sys_puts("cd: not a directory\n", 21, COLOR_TXT); return; }
    current_directory_inode = target;
    
    // Update current_path based on argument type
    if (argv[1][0] == '/') {
        // Absolute path
        strncpy(current_path, argv[1], 255);
        current_path[255] = 0;
    } else if (strcmp(argv[1], "..") == 0) {
        // Go up one level
        size_t len = strlen(current_path);
        if (len > 1) {
            if (current_path[len-1] == '/') len--;
            while (len > 1 && current_path[len-1] != '/') len--;
            current_path[len-1] = 0;
            if (len == 1) current_path[1] = 0;
        }
    } else if (strcmp(argv[1], ".") != 0) {
        // Regular relative path
        if (strcmp(current_path, "/") != 0) strcat(current_path, "/");
        strcat(current_path, argv[1]);
    }
}

static void cmd_cat(int argc, char* argv[]) {
    if (argc < 2) { sys_puts("cat: no argument\n", 17, COLOR_TXT); return; }
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);
    uint32_t parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("cat: parent not found\n", 23, COLOR_TXT); return; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = current_directory_inode;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("cat: parent not found\n", 23, COLOR_TXT); return; }
    }
    uint8_t filebuf[4096];
    struct EXT2DriverRequest req = { 
        .buf = filebuf, 
        .name = base, 
        .name_len = (uint8_t)strlen(base), 
        .parent_inode = parent_inode, 
        .buffer_size = sizeof(filebuf), 
        .is_folder = false 
    };
    int8_t rc = -1;
    rc = fs_readfile(&req, &rc);
    if (rc == 0) {
        for (uint32_t i = 0; i < sizeof(filebuf); i++) {
            char c = (char)filebuf[i];
            if (c == 0) break;
            putc_color(c, COLOR_TXT);
        }
        sys_putchar('\n', COLOR_TXT);
    } else {
        sys_puts("cat: error code ", 16, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
    }
}

static void cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) { sys_puts("mkdir: missing operand\n", 24, COLOR_TXT); return; }
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);
    if (base[0] == 0) { sys_puts("mkdir: invalid name\n", 21, COLOR_TXT); return; }
    uint32_t parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("mkdir: parent not found\n", 25, COLOR_TXT); return; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = current_directory_inode;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("mkdir: parent not found\n", 25, COLOR_TXT); return; }
    }
    struct EXT2DriverRequest req = { 
        .buf = 0, 
        .name = base, 
        .name_len = (uint8_t)strlen(base), 
        .parent_inode = parent_inode, 
        .buffer_size = 0, 
        .is_folder = true 
    };
    int8_t rc = -1;
    fs_write(&req, &rc);
    if (rc == 0) {
        sys_puts("mkdir: success\n", 15, COLOR_TXT);
        return;
    }
    sys_puts("mkdir: error code ", 18, COLOR_TXT);
    sys_putchar('0' + rc, COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
}

static void cmd_rm(int argc, char* argv[]) {
    if (argc < 2) { sys_puts("rm: missing operand\n", 20, COLOR_TXT); return; }
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);
    if (base[0] == 0) { sys_puts("rm: invalid name\n", 17, COLOR_TXT); return; }
    
    uint32_t parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("rm: parent not found\n", 21, COLOR_TXT); return; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = current_directory_inode;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("rm: parent not found\n", 21, COLOR_TXT); return; }
    }
    
    struct EXT2DriverRequest req = {
        .buf = 0,
        .name = base,
        .name_len = (uint8_t)strlen(base),
        .parent_inode = parent_inode,
        .buffer_size = 0,
        .is_folder = false
    };
    int8_t rc = -1;
    fs_delete(&req, &rc);
    if (rc != 0) {
        sys_puts("rm: error code ", 15, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
    }
}

static void cmd_cp(int argc, char* argv[]) {
    if (argc < 3) { sys_puts("cp: missing operand\n", 20, COLOR_TXT); return; }
    
    char src_parent[MAX_LINE], src_base[MAX_LINE];
    split_path(argv[1], src_parent, src_base);
    if (src_base[0] == 0) { sys_puts("cp: invalid source name\n", 24, COLOR_TXT); return; }
    
    uint32_t src_parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(src_parent, "/") == 0) src_parent_inode = 2;
        else if (!resolve_path(src_parent, &src_parent_inode)) { sys_puts("cp: source parent not found\n", 28, COLOR_TXT); return; }
    } else {
        if (strcmp(src_parent, ".") == 0) src_parent_inode = current_directory_inode;
        else if (!resolve_path(src_parent, &src_parent_inode)) { sys_puts("cp: source parent not found\n", 28, COLOR_TXT); return; }
    }
    
    uint8_t filebuf[4096];
    struct EXT2DriverRequest req_read = {
        .buf = filebuf,
        .name = src_base,
        .name_len = (uint8_t)strlen(src_base),
        .parent_inode = src_parent_inode,
        .buffer_size = sizeof(filebuf),
        .is_folder = false
    };
    int8_t rc = -1;
    rc = fs_readfile(&req_read, &rc);
    if (rc != 0) {
        sys_puts("cp: cannot read source\n", 23, COLOR_TXT);
        return;
    }
    
    char dst_parent[MAX_LINE], dst_base[MAX_LINE];
    split_path(argv[2], dst_parent, dst_base);
    if (dst_base[0] == 0) { sys_puts("cp: invalid dest name\n", 22, COLOR_TXT); return; }
    
    uint32_t dst_parent_inode;
    if (argv[2][0] == '/') {
        if (strcmp(dst_parent, "/") == 0) dst_parent_inode = 2;
        else if (!resolve_path(dst_parent, &dst_parent_inode)) { sys_puts("cp: dest parent not found\n", 26, COLOR_TXT); return; }
    } else {
        if (strcmp(dst_parent, ".") == 0) dst_parent_inode = current_directory_inode;
        else if (!resolve_path(dst_parent, &dst_parent_inode)) { sys_puts("cp: dest parent not found\n", 26, COLOR_TXT); return; }
    }
    
    struct EXT2DriverRequest req_write = {
        .buf = filebuf,
        .name = dst_base,
        .name_len = (uint8_t)strlen(dst_base),
        .parent_inode = dst_parent_inode,
        .buffer_size = 4096,
        .is_folder = false
    };
    rc = -1;
    fs_write(&req_write, &rc);
    if (rc != 0) {
        sys_puts("cp: error code ", 15, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
    }
}

static void cmd_mv(int argc, char* argv[]) {
    if (argc < 3) { sys_puts("mv: missing operand\n", 20, COLOR_TXT); return; }
    
    char src_parent[MAX_LINE], src_base[MAX_LINE];
    split_path(argv[1], src_parent, src_base);
    if (src_base[0] == 0) { sys_puts("mv: invalid source name\n", 24, COLOR_TXT); return; }
    
    uint32_t src_parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(src_parent, "/") == 0) src_parent_inode = 2;
        else if (!resolve_path(src_parent, &src_parent_inode)) { sys_puts("mv: source parent not found\n", 28, COLOR_TXT); return; }
    } else {
        if (strcmp(src_parent, ".") == 0) src_parent_inode = current_directory_inode;
        else if (!resolve_path(src_parent, &src_parent_inode)) { sys_puts("mv: source parent not found\n", 28, COLOR_TXT); return; }
    }
    
    uint8_t filebuf[4096];
    struct EXT2DriverRequest req_read = {
        .buf = filebuf,
        .name = src_base,
        .name_len = (uint8_t)strlen(src_base),
        .parent_inode = src_parent_inode,
        .buffer_size = sizeof(filebuf),
        .is_folder = false
    };
    int8_t rc = -1;
    rc = fs_readfile(&req_read, &rc);
    if (rc != 0) {
        sys_puts("mv: cannot read source\n", 23, COLOR_TXT);
        return;
    }
    
    char dst_parent[MAX_LINE], dst_base[MAX_LINE];
    split_path(argv[2], dst_parent, dst_base);
    if (dst_base[0] == 0) { sys_puts("mv: invalid dest name\n", 22, COLOR_TXT); return; }
    
    uint32_t dst_parent_inode;
    if (argv[2][0] == '/') {
        if (strcmp(dst_parent, "/") == 0) dst_parent_inode = 2;
        else if (!resolve_path(dst_parent, &dst_parent_inode)) { sys_puts("mv: dest parent not found\n", 26, COLOR_TXT); return; }
    } else {
        if (strcmp(dst_parent, ".") == 0) dst_parent_inode = current_directory_inode;
        else if (!resolve_path(dst_parent, &dst_parent_inode)) { sys_puts("mv: dest parent not found\n", 26, COLOR_TXT); return; }
    }
    
    struct EXT2DriverRequest req_write = {
        .buf = filebuf,
        .name = dst_base,
        .name_len = (uint8_t)strlen(dst_base),
        .parent_inode = dst_parent_inode,
        .buffer_size = 4096,
        .is_folder = false
    };
    rc = -1;
    fs_write(&req_write, &rc);
    if (rc != 0) {
        sys_puts("mv: write error code ", 21, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
        return;
    }
    
    struct EXT2DriverRequest req_del = {
        .buf = 0,
        .name = src_base,
        .name_len = (uint8_t)strlen(src_base),
        .parent_inode = src_parent_inode,
        .buffer_size = 0,
        .is_folder = false
    };
    rc = -1;
    fs_delete(&req_del, &rc);
    if (rc != 0) {
        sys_puts("mv: delete error code ", 22, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
    }
}

static void cmd_find_recursive(uint32_t dir_inode, const char* target, int depth) {
    if (depth > 10) return;
    
    uint8_t dirbuf[DIRBUF_BYTES];
    if (fetch_dir_table(dir_inode, dirbuf, sizeof(dirbuf)) != 0) return;
    
    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry e;
    char nm[256];
    
    while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
        if (strcmp(nm, target) == 0) {
            if (current_path[0] && strcmp(current_path, "/") != 0) {
                sys_puts(current_path, strlen(current_path), COLOR_TXT);
                sys_putchar('/', COLOR_TXT);
            }
            sys_puts(nm, strlen(nm), COLOR_TXT);
            sys_putchar('\n', COLOR_TXT);
        }
        
        if (e.file_type == EXT2_FT_DIR && strcmp(nm, ".") != 0 && strcmp(nm, "..") != 0) {
            char old_path[256];
            strcpy(old_path, current_path);
            if (strcmp(current_path, "/") != 0) strcat(current_path, "/");
            strcat(current_path, nm);
            cmd_find_recursive(e.inode, target, depth + 1);
            strcpy(current_path, old_path);
        }
    }
}

static void cmd_find(int argc, char* argv[]) {
    if (argc < 2) { sys_puts("find: missing pattern\n", 22, COLOR_TXT); return; }
    
    char saved_path[256];
    strcpy(saved_path, current_path);
    cmd_find_recursive(2, argv[1], 0);
    strcpy(current_path, saved_path);
}

static void cmd_grep(int argc, char* argv[]) {
    if (argc < 3) { sys_puts("grep: missing operand\n", 22, COLOR_TXT); return; }
    
    const char* pattern = argv[1];
    const char* filename = argv[2];
    
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(filename, parent_path, base);
    if (base[0] == 0) { sys_puts("grep: invalid file name\n", 24, COLOR_TXT); return; }
    
    uint32_t parent_inode;
    if (filename[0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("grep: file not found\n", 21, COLOR_TXT); return; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = current_directory_inode;
        else if (!resolve_path(parent_path, &parent_inode)) { sys_puts("grep: file not found\n", 21, COLOR_TXT); return; }
    }
    
    uint8_t filebuf[4096];
    struct EXT2DriverRequest req = {
        .buf = filebuf,
        .name = base,
        .name_len = (uint8_t)strlen(base),
        .parent_inode = parent_inode,
        .buffer_size = sizeof(filebuf),
        .is_folder = false
    };
    int8_t rc = -1;
    rc = fs_readfile(&req, &rc);
    if (rc != 0) {
        sys_puts("grep: cannot read file\n", 23, COLOR_TXT);
        return;
    }
    
    uint32_t line_start = 0;
    uint32_t pattern_len = strlen(pattern);
    for (uint32_t i = 0; i < 4096; i++) {
        char c = (char)filebuf[i];
        if (c == 0) {
            if (i > line_start) {
                uint32_t j;
                for (j = line_start; j <= i - pattern_len; j++) {
                    uint8_t match = 1;
                    for (uint32_t k = 0; k < pattern_len; k++) {
                        if ((char)filebuf[j + k] != pattern[k]) { match = 0; break; }
                    }
                    if (match) {
                        for (uint32_t k = line_start; k < i; k++) {
                            putc_color((char)filebuf[k], COLOR_TXT);
                        }
                        sys_putchar('\n', COLOR_TXT);
                        break;
                    }
                }
            }
            break;
        }
        
        if (c == '\n') {
            uint32_t line_len = i - line_start;
            if (line_len >= pattern_len) {
                uint32_t j;
                for (j = line_start; j <= i - pattern_len; j++) {
                    uint8_t match = 1;
                    for (uint32_t k = 0; k < pattern_len; k++) {
                        if ((char)filebuf[j + k] != pattern[k]) { match = 0; break; }
                    }
                    if (match) {
                        for (uint32_t k = line_start; k < i; k++) {
                            putc_color((char)filebuf[k], COLOR_TXT);
                        }
                        sys_putchar('\n', COLOR_TXT);
                        break;
                    }
                }
            }
            line_start = i + 1;
        }
    }
}

static void cmd_exit(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_puts("bye\n", 5, COLOR_TXT);
    while(1) {}
}

static void cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv;
    for (int i = 0; i < 25; i++) sys_putchar('\n', COLOR_TXT);
}

static int parse_command(char* line, char* argv[], int maxargs) {
    int n = 0;
    char* p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) { *p = 0; p++; }
        if (n == maxargs) break;
    }
    return n;
}

static void execute_command(int argc, char* argv[]) {
    if (argc <= 0) return;
    if (strcmp(argv[0], "pwd") == 0) cmd_pwd(argc, argv);
    else if (strcmp(argv[0], "help") == 0) cmd_help(argc, argv);
    else if (strcmp(argv[0], "ls") == 0) cmd_ls(argc, argv);
    else if (strcmp(argv[0], "cd") == 0) cmd_cd(argc, argv);
    else if (strcmp(argv[0], "cat") == 0) cmd_cat(argc, argv);
    else if (strcmp(argv[0], "mkdir") == 0) cmd_mkdir(argc, argv);
    else if (strcmp(argv[0], "rm") == 0) cmd_rm(argc, argv);
    else if (strcmp(argv[0], "cp") == 0) cmd_cp(argc, argv);
    else if (strcmp(argv[0], "mv") == 0) cmd_mv(argc, argv);
    else if (strcmp(argv[0], "find") == 0) cmd_find(argc, argv);
    else if (strcmp(argv[0], "grep") == 0) cmd_grep(argc, argv);
    else if (strcmp(argv[0], "clear") == 0) cmd_clear(argc, argv);
    else if (strcmp(argv[0], "exit") == 0) cmd_exit(argc, argv);
    else { sys_puts("undefined command: ", 19, COLOR_TXT); sys_puts(argv[0], strlen(argv[0]), COLOR_TXT); sys_putchar('\n', COLOR_TXT); }
}

int main(void) {
    char line[MAX_INPUT_LEN];
    char* argv[MAX_ARGS];
    int argc;
    int len = 0;

    sys_keyboard_activate();
    print_prompt();

    while (1) {
        char c = 0;
        sys_getchar(&c);
        if (!c) continue;

        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                sys_putchar('\b', COLOR_INPUT);
                sys_putchar(' ', COLOR_INPUT);
                sys_putchar('\b', COLOR_INPUT);
            }
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            sys_putchar('\n', COLOR_INPUT);
            line[len] = 0;
            argc = parse_command(line, argv, MAX_ARGS);
            if (argc > 0) execute_command(argc, argv);
            len = 0;
            print_prompt();
            continue;
        }
        
        if (c == 3) {
            sys_puts("^C\n", 3, COLOR_INPUT);
            len = 0;
            print_prompt();
            continue;
        }

        if (c >= 32 && c <= 126 && len < MAX_INPUT_LEN - 1) {
            sys_putchar(c, COLOR_INPUT);
            line[len++] = c;
        }
    }

    return 0;
}

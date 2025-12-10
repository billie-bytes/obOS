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

#define MAX_PATH_DIRS 8
#define MAX_PATH_STR  64
#define PATH_CONF_FILE "/path.conf"

static char path_dirs[MAX_PATH_DIRS][MAX_PATH_STR];
static int  path_dir_count = 0;

// ==== PATH helpers ====

// Directory traversal helper types used by autocomplete and path helpers
struct DirectoryTraversal {
    uint8_t* base;
    uint32_t size;
    uint32_t off;
};

static bool dirwalk_next(struct DirectoryTraversal* it, struct EXT2DirectoryEntry* out_entry, char* name_buf, uint16_t name_buf_sz);

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

static inline char read_key_blocking(void) {
    char k = 0; do { sys_getchar(&k); } while (!k); return k;
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

// Syscall 10: Process exit
static inline void sys_exit(void) {
    syscall_do(10, 0, 0, 0);
}

// Syscall 11: Create process (exec)
static inline int32_t sys_exec(struct EXT2DriverRequest* req) {
    int32_t retval;
    syscall_do(11, (uint32_t)req, 0, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

// Syscall 12: Kill process by PID
static inline int32_t sys_kill(uint32_t pid) {
    int32_t retval;
    syscall_do(12, pid, 0, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

// Syscall 13: Get process info (ps)
typedef enum {
    PROCESS_TERMINATED = 0,
    PROCESS_RUNNING = 1,
    PROCESS_READY = 2,
} PROCESS_STATE;

typedef struct {
    uint32_t pid;
    PROCESS_STATE state;
    char name[32];  // Changed from pointer to array to avoid kernel-user space issue
    uint8_t name_len;
} ProcessInfo;

static inline int32_t sys_ps(ProcessInfo *buffer, uint32_t bufsize) {
    int32_t count;
    syscall_do(13, (uint32_t)buffer, bufsize, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(count));
    return count;
}

// size_t strlen(const char *str) {
//     size_t len = 0;
//     while (str[len] != '\0') len++;
//     return len;
// }
extern size_t strlen(const char *str);
static void print_prompt(void) {
    sys_puts("jOSh@OS-IF2230", 14, COLOR_PROMPT_USER);
    sys_puts(":", 1, COLOR_PROMPT_SEP);
    sys_puts(current_path, strlen(current_path), COLOR_PROMPT_USER);
    sys_puts("$ ", 2, COLOR_PROMPT_SEP);
}
// history
#define HIST_MAX 32
static char history[HIST_MAX][MAX_INPUT_LEN];
static int  hist_len[HIST_MAX];
static int  hist_head = 0;   // next slot to write
static int  hist_count = 0;  // number of stored entries
static int  hist_nav = -1;   // -1 means not navigating, otherwise index 0..hist_count-1

static void history_add(const char* line, int len){
    if (len <= 0) return;
    if (len >= MAX_INPUT_LEN) len = MAX_INPUT_LEN-1;
    for (int i=0;i<len;i++) history[hist_head][i] = line[i];
    history[hist_head][len] = 0;
    hist_len[hist_head] = len;
    hist_head = (hist_head + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++; 
    hist_nav = -1;
}

static int history_get(int idx, char* out){
    if (idx < 0 || idx >= hist_count) return 0;
    int oldest = (hist_head - hist_count + HIST_MAX) % HIST_MAX;
    int slot = (oldest + idx) % HIST_MAX;
    int len = hist_len[slot];
    for (int i=0;i<len;i++) out[i] = history[slot][i];
    out[len] = 0;
    return len;
}

// Clear current 
static void erase_input_line(int len){
    for (int i=0;i<len;i++) { sys_putchar('\b', COLOR_INPUT); sys_putchar(' ', COLOR_INPUT); sys_putchar('\b', COLOR_INPUT); }
}

static int autocomplete(char* buf, int cur_len){
    // Find last token in buf
    int start = 0;
    for (int i=cur_len-1;i>=0;i--) { if (buf[i]==' '||buf[i]=='\t'){ start = i+1; break; } }
    const char* prefix = buf + start;
    int prefix_len = cur_len - start;

    // Detect if first word (command) or argument
    bool is_first_word = true;
    for (int i = 0; i < start; i++) {
        if (buf[i] != ' ' && buf[i] != '\t') { is_first_word = false; break; }
    }

    // Collect matches
    uint8_t dirbuf[DIRBUF_BYTES];
    int matches = 0;
    char match_name[256] = {0};
    uint8_t match_types[HIST_MAX];
    char match_list[HIST_MAX][256];

    if (is_first_word) {
        const char* builtins[] = { "ls","cd","pwd","cat","mkdir","exec","ps","kill","clear","help", "export", "exit" };
        int ncmd = (int)(sizeof(builtins)/sizeof(builtins[0]));
        for (int i=0;i<ncmd;i++) {
            if (prefix_len == 0 || strncmp(builtins[i], prefix, (size_t)prefix_len) == 0) {
                if (matches < HIST_MAX) {
                    strncpy(match_list[matches], builtins[i], sizeof(match_list[matches]));
                    match_types[matches] = EXT2_FT_REG_FILE; // treat as command
                    if (matches == 0) strncpy(match_name, builtins[i], sizeof(match_name));
                    matches++;
                }
            }
        }
    } else {
        // Read directory entries of current directory
        struct EXT2DriverRequest req = {
            .buf = dirbuf,
            .buffer_size = sizeof(dirbuf),
            .parent_inode = current_directory_inode,
            .name = ".",
            .name_len = 1,
            .is_folder = true
        };
        int8_t rc = -1;
        fs_readdir(&req, &rc);
        if (rc != 0) return 0;

        struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
        struct EXT2DirectoryEntry e;
        char nm[256];
        while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
            if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
            if (prefix_len == 0 || (int)strncmp(nm, prefix, (size_t)prefix_len) == 0) {
                if (matches < HIST_MAX) {
                    strncpy(match_list[matches], nm, sizeof(match_list[matches]));
                    match_types[matches] = e.file_type;
                    if (matches == 0) strncpy(match_name, nm, sizeof(match_name));
                    matches++;
                }
            }
        }
    }

    if (matches == 0) return 0;
    if (matches == 1) {
        // Complete the token
        int add = (int)strlen(match_name) - prefix_len;
        for (int i=0;i<add && cur_len < MAX_INPUT_LEN-1;i++) {
            buf[cur_len++] = match_name[prefix_len + i];
            sys_putchar(match_name[prefix_len + i], COLOR_INPUT);
        }
        // Add a trailing slash for directory args, space otherwise
        if (!is_first_word) {
            uint8_t t = match_types[0];
            if (t == EXT2_FT_DIR && cur_len < MAX_INPUT_LEN-1) { buf[cur_len++] = '/'; sys_putchar('/', COLOR_INPUT); }
            else if (cur_len < MAX_INPUT_LEN-1) { buf[cur_len++] = ' '; sys_putchar(' ', COLOR_INPUT); }
        }
        return cur_len;
    }
    // Multiple matches: print newline, list, then reprint prompt + line
    sys_putchar('\n', COLOR_TXT);
    for (int i=0;i<matches;i++) {
        uint8_t color = (!is_first_word && match_types[i] == EXT2_FT_DIR) ? COLOR_DIR : COLOR_TXT;
        sys_puts(match_list[i], strlen(match_list[i]), color);
        if (!is_first_word && match_types[i] == EXT2_FT_DIR) sys_putchar('/', COLOR_DIR);
        sys_putchar(' ', COLOR_TXT);
    }
    sys_putchar('\n', COLOR_TXT);
    print_prompt();
    sys_puts(buf, cur_len, COLOR_INPUT);
    return cur_len;
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

static void path_clear(void) {
    path_dir_count = 0;
}

static int path_index_of(const char *dir) {
    for (int i = 0; i < path_dir_count; i++) {
        if (strcmp(path_dirs[i], dir) == 0) return i;
    }
    return -1;
}

static int path_add(const char *dir) {
    if (!dir || !*dir) return -1;
    if (path_index_of(dir) >= 0) return 0;
    if (path_dir_count >= MAX_PATH_DIRS) return -1;

    strncpy(path_dirs[path_dir_count], dir, MAX_PATH_STR - 1);
    path_dirs[path_dir_count][MAX_PATH_STR - 1] = 0;
    path_dir_count++;
    return 0;
}

static int path_remove(const char *dir) {
    int idx = path_index_of(dir);
    if (idx < 0) return -1;
    for (int i = idx + 1; i < path_dir_count; i++) {
        strcpy(path_dirs[i - 1], path_dirs[i]);
    }
    path_dir_count--;
    return 0;
}

// Return parent inode and basename for PATH_CONF_FILE
static bool get_path_conf_parent(uint32_t *parent_inode_out, char *base_out) {
    char parent_path[MAX_LINE];
    split_path(PATH_CONF_FILE, parent_path, base_out);
    // PATH_CONF_FILE is absolute
    if (strcmp(parent_path, "/") == 0) {
        *parent_inode_out = 2;   // root inode
        return true;
    }
    return resolve_path(parent_path, parent_inode_out);
}

static void save_path_to_disk(void) {
    uint32_t parent_inode;
    char base[MAX_LINE];

    if (!get_path_conf_parent(&parent_inode, base)) return;

    uint8_t buf[512];
    uint32_t pos = 0;

    for (int i = 0; i < path_dir_count; i++) {
        size_t L = strlen(path_dirs[i]);
        if (pos + L + 2 >= sizeof(buf)) break; // avoid overflow
        memcpy(buf + pos, path_dirs[i], L);
        pos += (uint32_t)L;
        buf[pos++] = '\n';
    }
    if (pos == 0) {
        buf[0] = 0;
        pos = 1;
    } else {
        buf[pos] = 0;
        pos++;
    }

    struct EXT2DriverRequest req = {
        .buf          = buf,
        .name         = base,
        .name_len     = (uint8_t)strlen(base),
        .parent_inode = parent_inode,
        .buffer_size  = pos,
        .is_folder    = false
    };
    int8_t rc = -1;
    fs_write(&req, &rc);
    (void)rc;
}

static void load_path_from_disk(void) {
    path_clear();

    uint32_t parent_inode;
    char base[MAX_LINE];

    if (!get_path_conf_parent(&parent_inode, base)) {
        path_add("/bin");
        return;
    }

    uint8_t buf[512];
    struct EXT2DriverRequest req = {
        .buf          = buf,
        .name         = base,
        .name_len     = (uint8_t)strlen(base),
        .parent_inode = parent_inode,
        .buffer_size  = sizeof(buf),
        .is_folder    = false
    };
    int8_t rc = -1;
    fs_readfile(&req, &rc);
    if (rc != 0) {
        path_add("/bin");
        return;
    }

    uint32_t i = 0;
    while (i < sizeof(buf) && buf[i] != 0) {
        char tmp[MAX_PATH_STR];
        uint32_t j = 0;
        while (i < sizeof(buf) && buf[i] != '\n' && buf[i] != 0 && j < MAX_PATH_STR - 1) {
            tmp[j++] = (char)buf[i++];
        }
        tmp[j] = 0;
        if (j > 0) path_add(tmp);
        if (i < sizeof(buf) && buf[i] == '\n') i++;
    }

    if (path_dir_count == 0) {
        path_add("/bin");
    }
}

static void path_print(void) {
    for (int i = 0; i < path_dir_count; i++) {
        sys_puts(path_dirs[i], strlen(path_dirs[i]), COLOR_TXT);
        if (i + 1 < path_dir_count)
            sys_puts(":", 1, COLOR_TXT);
    }
    sys_putchar('\n', COLOR_TXT);
}


static void cmd_export(int argc, char* argv[]) {
    if (argc == 1) {
        path_print();
        return;
    }
    if (argc < 3) {
        sys_puts("usage: export add|del <DIR>\n", 30, COLOR_TXT);
        return;
    }

    const char *op  = argv[1];
    const char *dir = argv[2];

    if (strcmp(op, "add") == 0) {
        if (path_add(dir) == 0) {
            save_path_to_disk();
        } else {
            sys_puts("export: cannot add PATH entry\n", 32, COLOR_TXT);
        }
    } else if (strcmp(op, "del") == 0) {
        if (path_remove(dir) == 0) {
            save_path_to_disk();
        } else {
            sys_puts("export: directory not in PATH\n", 33, COLOR_TXT);
        }
    } else {
        sys_puts("usage: export add|del <DIR>\n", 30, COLOR_TXT);
    }
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
    sys_puts("  exec <PROGRAM>\n", 17, COLOR_TXT);
    sys_puts("  ps\n", 5, COLOR_TXT);
    sys_puts("  kill <PID>\n", 13, COLOR_TXT);
    sys_puts("  clear\n", 8, COLOR_TXT);
    sys_puts("  export [add|del] <DIR>\n", 26, COLOR_TXT);
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

static int spawn_program_at(const char *prog_path) {
    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(prog_path, parent_path, base);

    uint32_t parent_inode;

    if (prog_path[0] == '/') {
        if (strcmp(parent_path, "/") == 0)
            parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) {
            sys_puts("exec: parent not found\n", 23, COLOR_TXT);
            return -1;
        }
    } else {
        if (strcmp(parent_path, ".") == 0)
            parent_inode = current_directory_inode;
        else if (!resolve_path(parent_path, &parent_inode)) {
            sys_puts("exec: parent not found\n", 23, COLOR_TXT);
            return -1;
        }
    }

    uint32_t process_buffer = (2 * 1024 * 1024);
    struct EXT2DriverRequest req = {
        .buf          = (uint8_t*)0,
        .name         = base,
        .name_len     = (uint8_t)strlen(base),
        .parent_inode = parent_inode,
        .buffer_size  = process_buffer,
        .is_folder    = false
    };

    int32_t res = sys_exec(&req);
    return res;
}


static void cmd_exec(int argc, char* argv[]) {
    if (argc < 2) { 
        sys_puts("exec: missing program name\n", 28, COLOR_TXT); 
        return; 
    }
    (void)spawn_program_at(argv[1]);
}

static bool try_exec_with_path(char* argv[]) {
    const char *cmd = argv[0];

    // If command already contains '/', treat it as a direct path
    if (strchr(cmd, '/') != 0) {
        return (spawn_program_at(cmd) == 0);
    }

    for (int i = 0; i < path_dir_count; i++) {
        char full[MAX_LINE];
        strncpy(full, path_dirs[i], MAX_LINE - 1);
        full[MAX_LINE - 1] = 0;

        size_t L = strlen(full);
        if (L == 0) continue;

        if (full[L-1] != '/' && L < MAX_LINE - 1) {
            full[L]   = '/';
            full[L+1] = 0;
            L++;
        }

        if (L + strlen(cmd) >= MAX_LINE) continue;
        strcat(full, cmd);

        if (spawn_program_at(full) == 0) {
            return true;    // success
        }
    }
    return false; // nothing worked
}

static void cmd_ps(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    ProcessInfo proc_list[16];  // Max 16 processes
    int32_t count = sys_ps(proc_list, 16);
    
    if (count <= 0) {
        sys_puts("ps: no processes found\n", 23, COLOR_TXT);
        return;
    }
    
    // Print header
    sys_puts("PID  STATE    NAME", 18, COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
    sys_puts("---  -------  ----\n", 24, COLOR_TXT);
    
    for (int i = 0; i < count; i++) {
        // Print PID (2 digits)
        if (proc_list[i].pid < 10) {
            sys_putchar(' ', COLOR_TXT);
        }
        sys_putchar('0' + (proc_list[i].pid / 10), COLOR_TXT);
        sys_putchar('0' + (proc_list[i].pid % 10), COLOR_TXT);
        sys_puts("   ", 3, COLOR_TXT);
        
        // Print state
        if (proc_list[i].state == PROCESS_RUNNING) {
            sys_puts("RUNNING ", 8, COLOR_TXT);
        } else if (proc_list[i].state == PROCESS_READY) {
            sys_puts("READY   ", 8, COLOR_TXT);
        } else {
            sys_puts("TERM    ", 8, COLOR_TXT);
        }
        sys_puts(" ", 1, COLOR_TXT);
        
        // Print name (directly from array, already copied from kernel space)
        if (proc_list[i].name_len > 0) {
            uint32_t len = proc_list[i].name_len;
            if (len > 8) len = 8;
            sys_puts(proc_list[i].name, len, COLOR_TXT);
        }
        sys_putchar('\n', COLOR_TXT);
    }
}

static void cmd_kill(int argc, char* argv[]) {
    if (argc < 2) {
        sys_puts("kill: missing PID\n", 18, COLOR_TXT);
        return;
    }
    
    // Simple PID parsing (assumes decimal number)
    uint32_t pid = 0;
    for (char* p = argv[1]; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            pid = pid * 10 + (*p - '0');
        } else {
            sys_puts("kill: invalid PID\n", 18, COLOR_TXT);
            return;
        }
    }
    
    int32_t result = sys_kill(pid);
    
    if (result == 0) {
        sys_puts("kill: process ", 14, COLOR_TXT);
        sys_puts(argv[1], strlen(argv[1]), COLOR_TXT);
        sys_puts(" terminated\n", 12, COLOR_TXT);
    } else {
        sys_puts("kill: process not found or cannot be killed\n", 45, COLOR_TXT);
    }
}

static void cmd_exit(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sys_puts("bye\n", 5, COLOR_TXT);
    while(1) {}
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
    else if (strcmp(argv[0], "exec") == 0) cmd_exec(argc, argv);
    else if (strcmp(argv[0], "ps") == 0) cmd_ps(argc, argv);
    else if (strcmp(argv[0], "kill") == 0) cmd_kill(argc, argv);
    else if (strcmp(argv[0], "clear") == 0) { __asm__ volatile("mov $8, %eax; int $0x30"); }
    else if (strcmp(argv[0], "export") == 0) { cmd_export(argc, argv); }
    else if (strcmp(argv[0], "exit") == 0) cmd_exit(argc, argv);
    else {
        if (!try_exec_with_path(argv)) {
            sys_puts("undefined command: ", 19, COLOR_TXT);
            sys_puts(argv[0], strlen(argv[0]), COLOR_TXT);
            sys_putchar('\n', COLOR_TXT);
        }
    }
}

int main(void) {
    char line[MAX_INPUT_LEN];
    char* argv[MAX_ARGS];
    int argc;
    int len = 0;

    sys_keyboard_activate();
    load_path_from_disk();
    print_prompt();

    while (1) {
        char c = 0;
        sys_getchar(&c);
        if (!c) continue;
        // ANSI escape sequence for Up/Down (ESC [ A/B)
        if (c == 0x1B) {
            char b1 = read_key_blocking();
            if (b1 == '[') {
                char b2 = read_key_blocking();
                if (b2 == 'A' && hist_count > 0) {
                    if (hist_nav < 0) hist_nav = hist_count - 1; else if (hist_nav > 0) hist_nav--;
                    char tmp[MAX_INPUT_LEN]; int hlen = history_get(hist_nav, tmp);
                    erase_input_line(len);
                    {
                        for (int i=0;i<hlen;i++) line[i] = tmp[i];
                        len = hlen;
                        line[len] = 0;
                        sys_puts(line, len, COLOR_INPUT);
                    }
                    continue;
                }
                if (b2 == 'B' && hist_count > 0) {
                    if (hist_nav >= 0) {
                        hist_nav++;
                    }
                    if (hist_nav >= hist_count) {
                        hist_nav = -1;
                    }
                    erase_input_line(len);
                    if (hist_nav >= 0) { char tmp[MAX_INPUT_LEN]; int hlen = history_get(hist_nav, tmp); for (int i=0;i<hlen;i++) line[i] = tmp[i]; len = hlen; line[len] = 0; sys_puts(line, len, COLOR_INPUT); }
                    else { len = 0; line[0] = 0; }
                    continue;
                }
                continue;
            }
            continue;
        }
        if ((unsigned char)c == 0x80 /*KEY_UP*/ && hist_count > 0) {
            if (hist_nav < 0) hist_nav = hist_count - 1; else if (hist_nav > 0) hist_nav--;
            char tmp[MAX_INPUT_LEN]; int hlen = history_get(hist_nav, tmp);
            erase_input_line(len);
            for (int i=0;i<hlen;i++) line[i] = tmp[i];
            len = hlen; line[len] = 0;
            sys_puts(line, len, COLOR_INPUT);
            continue;
        }
        if ((unsigned char)c == 0x81 /*KEY_DOWN*/ && hist_count > 0) {
            if (hist_nav >= 0) {
                hist_nav++;
            }
            if (hist_nav >= hist_count) {
                hist_nav = -1;
            }
            // -1 means empty current line
            erase_input_line(len);
            if (hist_nav >= 0) { char tmp[MAX_INPUT_LEN]; int hlen = history_get(hist_nav, tmp);
                {
                    for (int i=0;i<hlen;i++) line[i] = tmp[i];
                    len = hlen;
                    line[len]=0;
                    sys_puts(line, len, COLOR_INPUT);
                }
            } else { len = 0; line[0]=0; }
            continue;
        }
        // Tab autocomplete
        if (c == '\t') { len = autocomplete(line, len); hist_nav = -1; continue; }

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
            if (argc > 0) { execute_command(argc, argv); history_add(line, len); }
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
            hist_nav = -1;
        }
    }

    return 0;
}

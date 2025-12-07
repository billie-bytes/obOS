#include <stdint.h>
#include <stdbool.h>
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"

#define MAX_LINE        256
#define MAX_ARGS        16
#define COLOR_TXT       0x0A
#define COLOR_DIR       0x09
#define BLOCK_COUNT     16
#define DIRBUF_BYTES    (BLOCK_COUNT * BLOCK_SIZE)

static inline void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx"::"r"(ebx));
    __asm__ volatile("mov %0, %%ecx"::"r"(ecx));
    __asm__ volatile("mov %0, %%edx"::"r"(edx));
    __asm__ volatile("mov %0, %%eax"::"r"(eax));
    __asm__ volatile("int $0x30");
}

static inline void putc_color(char c, uint8_t fg) { syscall(5u, (uint32_t)(uint8_t)c, (uint32_t)fg, 0u); }
static inline void puts_color(const char* s, uint8_t fg, uint32_t n) {
    syscall(6u, (uint32_t)s, n, (uint32_t)fg);
}

void print(const char* s, uint8_t color) {
    if (s) puts_color(s, color, (uint32_t)strlen(s));
}

void print_char(char c, uint8_t color) {
    putc_color(c, color);
}

// Keep old name for compatibility with existing code
static void print_str(const char* s, uint8_t color) {
    print(s, color);
}
static inline char getchar_blocking(void) {
    char c = 0; do { syscall(4u, (uint32_t)&c, 0, 0); } while (c == 0); return c;
}
static inline void kbd_activate(void)                               { syscall(7u, 0, 0, 0); }
static inline void set_cursor_pos(uint8_t x, uint8_t y)             { syscall(16u, x, y, 0); }
static inline void get_cursor_pos(uint8_t* x, uint8_t* y)           { syscall(17u, (uint32_t)x, (uint32_t)y, 0); }
static inline void clear_screen(void)                               { syscall(8u, 0, 0, 0); }

static inline int8_t fs_readfile(struct EXT2DriverRequest* r, int8_t* rc)    { syscall(0, (uint32_t)r, (uint32_t)rc, 0); return *rc; }
static inline int8_t fs_readdir(struct EXT2DriverRequest* r, int8_t* rc)     { syscall(1, (uint32_t)r, (uint32_t)rc, 0); return *rc; }
static inline int8_t fs_write(struct EXT2DriverRequest* r, int8_t* rc)       { syscall(2, (uint32_t)r, (uint32_t)rc, 0); return *rc; }
static inline int8_t fs_delete(struct EXT2DriverRequest* r, int8_t* rc)      { syscall(3, (uint32_t)r, (uint32_t)rc, 0); return *rc; }

static uint32_t g_cwd_inode = 2;
static char     g_cwd_path[MAX_LINE] = "/";

int readline(char* buf, int maxlen) {
    int len = 0;
    while (1) {
        char c = getchar_blocking();
        if (c == '\r' || c == '\n') { putc_color('\n', COLOR_TXT); buf[len] = 0; return len; }
        if ((c == '\b' || c == 127) && len > 0) {
            putc_color('\b', COLOR_TXT); putc_color(' ', COLOR_TXT); putc_color('\b', COLOR_TXT);
            len--; continue;
        }
        if (c >= 32 && c <= 126 && len < maxlen - 1) {
            putc_color(c, COLOR_TXT);
            buf[len++] = c;
        }
    }
}

static int parse(char* line, char* argv[], int maxargs) {
    int n = 0; char* p = line;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
        if (n == maxargs) break;
    }
    return n;
}

/* Split path into parent path + basename. */
static void split_path(const char* in, char* parent_out, char* base_out) {
    if (!in || !*in) { strcpy(parent_out, "."); strcpy(base_out, ""); return; }

    char tmp[MAX_LINE]; strncpy(tmp, in, sizeof(tmp)); tmp[sizeof(tmp)-1] = 0;

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

/* ========= Directory entry walking ========= */
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
        .buffer_size  = buf_sz,
        .parent_inode = dir_inode,
        .name         = ".",
        .name_len     = 1,
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

// check if path exist
static bool resolve_path(const char* path, uint32_t* out_inode) {
    if (!path || !*path) { *out_inode = g_cwd_inode; return true; }
    if (strcmp(path, "/") == 0) { *out_inode = 2; return true; }

    uint32_t cur = (path[0] == '/') ? 2 : g_cwd_inode;

    char tmp[MAX_LINE]; strncpy(tmp, path, sizeof(tmp)); tmp[sizeof(tmp)-1] = 0;

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

void print_prompt(void) {
    print_str("jOSh@OS-IF2230", 0x0A);  /* light green */
    print_str(":",               0x07);  /* gray */
    print_str(" ",               0x07);
    print_str(g_cwd_path,        COLOR_TXT);
    print_str("$ ",              0x07);
}


static void cmd_pwd(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_str(g_cwd_path, COLOR_TXT);
    print_str("\n", COLOR_TXT);
}

static void cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv;
    for (int i = 0; i < 40; i++) print_str("\n", COLOR_TXT);
}

static void cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_str("Commands:\n", COLOR_TXT);
    print_str("  ls [PATH]\n", COLOR_TXT);
    print_str("  cd [PATH]\n", COLOR_TXT);
    print_str("  pwd\n", COLOR_TXT);
    print_str("  cat <FILE>\n", COLOR_TXT);
    print_str("  mkdir <DIR>\n", COLOR_TXT);
    print_str("  clear\n", COLOR_TXT);
    print_str("  help\n", COLOR_TXT);
    print_str("  exit\n", COLOR_TXT);
}

static void cmd_ls(int argc, char* argv[]) {
    uint32_t target = g_cwd_inode;

    if (argc > 2) { print_str("ls: too many arguments\n", COLOR_TXT); return; }
    if (argc == 2) {
        if (!resolve_path(argv[1], &target)) { print_str("ls: no such file or directory\n", COLOR_TXT); return; }
    }

    uint8_t dirbuf[DIRBUF_BYTES];
    if (fetch_dir_table(target, dirbuf, sizeof(dirbuf)) != 0) { print_str("ls: not a directory\n", COLOR_TXT); return; }

    struct DirectoryTraversal it = { .base = dirbuf, .size = sizeof(dirbuf), .off = 0 };
    struct EXT2DirectoryEntry e;
    char nm[256];
    bool any = false;

    while (dirwalk_next(&it, &e, nm, sizeof(nm))) {
        if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
        uint8_t color = (e.file_type == EXT2_FT_DIR) ? COLOR_DIR : COLOR_TXT;
        puts_color(nm, color, (uint32_t)strlen(nm));
        print_str("\n", COLOR_TXT);
        any = true;
    }
    if (!any) print_str("\n", COLOR_TXT);
}

static void path_set(const char* s) {
    size_t L = strlen(s);
    if (L >= sizeof(g_cwd_path)) L = sizeof(g_cwd_path) - 1;
    strncpy(g_cwd_path, s, L);
    g_cwd_path[L] = '\0';

    while (L > 1 && g_cwd_path[L-1] == '/') {
        g_cwd_path[--L] = '\0';
    }
}

/* Join relative segment to g_cwd_path */
static void path_join_rel(const char* seg) {
    size_t baseL = strlen(g_cwd_path);
    size_t segL  = strlen(seg);

    char tmp[MAX_LINE];
    strncpy(tmp, g_cwd_path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';

    if (strcmp(tmp, "/") == 0) {
        if (1 + segL >= sizeof(tmp)) segL = sizeof(tmp) - 1 - 1;
        tmp[0] = '/'; tmp[1] = '\0';
        strncat(tmp, seg, segL);
    } else {
        if (baseL + 1 < sizeof(tmp)) {
            tmp[baseL] = '/';
            tmp[baseL+1] = '\0';
            if (baseL + 1 + segL >= sizeof(tmp))
                segL = sizeof(tmp) - 1 - (baseL + 1);
            strncat(tmp, seg, segL);
        }
    }

    size_t L = strlen(tmp);
    while (L > 1 && tmp[L-1] == '/') tmp[--L] = '\0';

    strncpy(g_cwd_path, tmp, sizeof(g_cwd_path)-1);
    g_cwd_path[sizeof(g_cwd_path)-1] = '\0';
}

static void cmd_cd(int argc, char* argv[]) {
    if (argc < 2) { g_cwd_inode = 2; strcpy(g_cwd_path, "/"); return; }
    uint32_t target;
    if (!resolve_path(argv[1], &target)) { print_str("cd: no such file or directory\n", COLOR_TXT); return; }

    uint8_t dummy[BLOCK_SIZE];
    if (fetch_dir_table(target, dummy, sizeof(dummy)) != 0) { print_str("cd: not a directory\n", COLOR_TXT); return; }

    g_cwd_inode = target;

    if (argv[1][0] == '/') path_set(argv[1]);
    else path_join_rel(argv[1]);
}

static void cmd_cat(int argc, char* argv[]) {
    if (argc < 2) { print_str("cat: no argument\n", COLOR_TXT); return; }

    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);

    uint32_t parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { print_str("cat: parent not found\n", COLOR_TXT); return; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = g_cwd_inode;
        else if (!resolve_path(parent_path, &parent_inode)) { print_str("cat: parent not found\n", COLOR_TXT); return; }
    }

    uint8_t filebuf[4096];
    struct EXT2DriverRequest req = {
        .buf          = filebuf,
        .buffer_size  = sizeof(filebuf),
        .parent_inode = parent_inode,
        .name         = base,
        .name_len     = (uint32_t)strlen(base),
        .is_folder    = false
    };
    int8_t rc = -1;
    rc = fs_readfile(&req, &rc);

    if (rc == 0) {
        for (uint32_t i = 0; i < sizeof(filebuf); i++) {
            char c = (char)filebuf[i];
            if (c == 0) break;
            putc_color(c, COLOR_TXT);
        }
        print_str("\n", COLOR_TXT);
    } else if (rc == 1) {
        print_str("cat: not a file\n", COLOR_TXT);
    } else if (rc == 2) {
        print_str("cat: buffer too small\n", COLOR_TXT);
    } else if (rc == 3) {
        print_str("cat: not found\n", COLOR_TXT);
    } else {
        print_str("cat: error\n", COLOR_TXT);
    }
}

static void cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) { print_str("mkdir: missing operand\n", COLOR_TXT); return; }

    char parent_path[MAX_LINE], base[MAX_LINE];
    split_path(argv[1], parent_path, base);

    if (base[0] == 0) { print_str("mkdir: invalid name\n", COLOR_TXT); return; }

    uint32_t parent_inode;
    if (argv[1][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { print_str("mkdir: parent not found\n", COLOR_TXT); return; }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = g_cwd_inode;
        else if (!resolve_path(parent_path, &parent_inode)) { print_str("mkdir: parent not found\n", COLOR_TXT); return; }
    }

    struct EXT2DriverRequest req = {
        .buf          = 0,
        .buffer_size  = 0,
        .parent_inode = parent_inode,
        .name         = base,
        .name_len     = (uint32_t)strlen(base),
        .is_folder    = true
    };
    int8_t rc = -1;
    fs_write(&req, &rc);
    if (rc == 0) return;
    if (rc == 1) print_str("mkdir: already exists\n", COLOR_TXT);
    else if (rc == 2) print_str("mkdir: invalid parent\n", COLOR_TXT);
    else print_str("mkdir: error\n", COLOR_TXT);
}

int read_line(char* out, int out_max){
    return readline(out, out_max);
}

void parse_command(char* line, char* argv[], int* argc_out) {
    *argc_out = parse(line, argv, MAX_ARGS);
}

void execute_command(int argc, char* argv[]) {
    if (argc <= 0) return;
    if (strcmp(argv[0], "pwd")   == 0) { cmd_pwd(argc, argv); return; }
    if (strcmp(argv[0], "clear") == 0) { cmd_clear(argc, argv); return; }
    if (strcmp(argv[0], "help")  == 0) { cmd_help(argc, argv); return; }
    if (strcmp(argv[0], "ls")    == 0) { cmd_ls(argc, argv); return; }
    if (strcmp(argv[0], "cd")    == 0) { cmd_cd(argc, argv); return; }
    if (strcmp(argv[0], "cat")   == 0) { cmd_cat(argc, argv); return; }
    if (strcmp(argv[0], "mkdir") == 0) { cmd_mkdir(argc, argv); return; }

    if (strcmp(argv[0], "exit")  == 0) {
        print_str("bye\n", COLOR_TXT);
        for (;;) {}
    }

    print_str("undefined command: ", COLOR_TXT);
    print_str(argv[0], COLOR_TXT);
    print_str("\n", COLOR_TXT);
}

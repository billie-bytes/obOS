#include "syscall.h"
#include "../header/stdlib/string.h"

#define MAX_LINE 256
#define DIRBUF_BYTES (BLOCK_SIZE * 8)

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
extern void* memset(void* s, int c, size_t n);

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

static int kstrstr(const char *line_start, const char *pattern, int len) {
    int pattern_len = strlen(pattern);
    if (pattern_len == 0) return 1;
    
    for (int i = 0; i <= len - pattern_len; i++) {
        int j;
        for (j = 0; j < pattern_len; j++) {
            if (line_start[i + j] != pattern[j]) {
                break;
            }
        }
        if (j == pattern_len) return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) { 
        sys_puts("grep: missing operand\n", 22, COLOR_TXT); 
        return 1; 
    }
    
    char parent_path[256];
    char base[256];
    uint32_t parent_inode = 0;
    static char filebuf[4096]; 
    memset(filebuf, 0, 4096);
    char pattern_buf[256];
    memset(pattern_buf, 0, 256);
    char* final_pattern = pattern_buf;
    int current_word = 1;
    
    if (current_word >= argc - 1) {
        sys_puts("grep: missing pattern or file\n", 28, COLOR_TXT);
        return 1;
    }
    
    if (argv[current_word][0] == '"') {
        while (current_word < argc) {
            strcat(pattern_buf, argv[current_word]);
            int len = strlen(argv[current_word]);
            if (len > 0 && argv[current_word][len - 1] == '"') {
                break; 
            }
            strcat(pattern_buf, " ");
            current_word++;
        }
        int total_len = strlen(pattern_buf);
        if (total_len > 0 && pattern_buf[total_len - 1] == '"') {
            pattern_buf[total_len - 1] = '\0';
        }
        if (pattern_buf[0] == '"') {
            final_pattern++; 
        }
    } 
    else {
        strcpy(pattern_buf, argv[current_word]);
    }
    current_word++;

    if (current_word >= argc) {
        sys_puts("grep: missing filename\n", 23, COLOR_TXT);
        return 1;
    }

    split_path(argv[current_word], parent_path, base);
    if (argv[current_word][0] == '/') {
        if (strcmp(parent_path, "/") == 0) parent_inode = 2;
        else if (!resolve_path(parent_path, &parent_inode)) { 
            sys_puts("grep: parent not found\n", 23, COLOR_TXT); 
            return 1; 
        }
    } else {
        if (strcmp(parent_path, ".") == 0) parent_inode = sys_getcwd_inode();
        else if (!resolve_path(parent_path, &parent_inode)) { 
            sys_puts("grep: parent not found\n", 23, COLOR_TXT); 
            return 1; 
        }
    }
    
    struct EXT2DriverRequest req = { 
        .buf = filebuf, 
        .name = base, 
        .name_len = (uint8_t)strlen(base), 
        .parent_inode = parent_inode, 
        .buffer_size = sizeof(filebuf), 
        .is_folder = false 
    };
    int8_t rc = sys_read(&req);
    
    if (rc == 0) {
        char* line_start = filebuf;
        int buf_len = 4096;
        
        for (int i = 0; i < buf_len; i++) {
            if (filebuf[i] == '\n' || filebuf[i] == '\0') {
                int line_length = (int)(&filebuf[i] - line_start);
                char temp_saver = filebuf[i];
                filebuf[i] = '\0';
                if (kstrstr(line_start, final_pattern, line_length)) {
                    sys_puts(line_start, line_length, COLOR_TXT);
                    sys_puts("\n", 1, COLOR_TXT);
                }
                filebuf[i] = temp_saver;
                line_start = &filebuf[i+1];
                if (temp_saver == '\0') break;
            }
        }
    } else {
        sys_puts("grep: error code ", 16, COLOR_TXT);
        sys_putchar('0' + rc, COLOR_TXT);
        sys_putchar('\n', COLOR_TXT);
        return 1;
    }
    
    return 0;
}

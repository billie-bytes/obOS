#include "user/command/syscall.h"
#include "lib/string.h"

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
    
    // Pattern Parsing
    if (argv[current_word][0] == '"') {
        while (current_word < argc) {
            strcat(pattern_buf, argv[current_word]);
            int len = strlen(argv[current_word]);
            if (len > 0 && argv[current_word][len - 1] == '"') break; 
            strcat(pattern_buf, " ");
            current_word++;
        }
        int total_len = strlen(pattern_buf);
        if (total_len > 0 && pattern_buf[total_len - 1] == '"') {
            pattern_buf[total_len - 1] = '\0';
        }
        if (pattern_buf[0] == '"') final_pattern++; 
    } else {
        strcpy(pattern_buf, argv[current_word]);
    }
    current_word++;

    if (current_word >= argc) {
        sys_puts("grep: missing filename\n", 23, COLOR_TXT);
        return 1;
    }

    const char* target_path = argv[current_word];
    uint8_t type = 0;
    
    // Fast path resolution via kernel
    uint32_t target_inode = sys_stat(target_path, &type);
    
    if (target_inode == 0) {
        sys_puts("grep: file not found\n", 21, COLOR_TXT);
        return 1;
    }
    if (type == EXT2_FT_DIR) {
        sys_puts("grep: is a directory\n", 21, COLOR_TXT);
        return 1;
    }

    // Direct data read
    int32_t bytes_read = sys_read(target_inode, filebuf, sizeof(filebuf));
    
    if (bytes_read >= 0) {
        char* line_start = filebuf;
        for (int i = 0; i < bytes_read; i++) {
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
        sys_puts("grep: error reading file\n", 25, COLOR_TXT);
        return 1;
    }
    
    return 0;
}
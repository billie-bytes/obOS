#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include "header/text/badapple.h"
#include "command/syscall.h"

static inline void sys_sleep(uint32_t ms) {
    syscall_do(9u, ms, 0, 0);
}

// Retained inline asm here because syscall_do only takes eax, ebx, ecx, edx, but this needs edi
static inline void sys_putchar_at(uint8_t row, uint8_t col, char c, uint8_t color) {
    __asm__ volatile("mov %0, %%ebx" : : "r"((uint32_t)row));
    __asm__ volatile("mov %0, %%ecx" : : "r"((uint32_t)col));
    __asm__ volatile("mov %0, %%edx" : : "r"((uint32_t)(uint8_t)c));
    __asm__ volatile("mov %0, %%edi" : : "r"((uint32_t)color));
    __asm__ volatile("mov $15, %%eax" : : );
    __asm__ volatile("int $0x30");
}

#define COLOR_VIDEO      0x0F 
#define FRAME_COLS       80
#define FRAME_ROWS       25
#define FRAME_DELAY_MS   40 
#define CTRL_C_CODE      3 

#define ANIM_BUF_SIZE    (2 * 1024 * 1024)  /* 1.9 MiB */

#define ANIM_FILENAME    "badapple.txt"
#define ANIM_NAME_LEN    12

static uint8_t anim_buf[ANIM_BUF_SIZE];

static bool check_ctrl_c(void) {
    char k = 0;

    do {
        sys_getchar(&k);
        if (k == CTRL_C_CODE) {
            sys_clear();
            sys_exit();
            return true;
        }
    } while (k != 0);

    return false;
}

static void draw_frame(char frame[FRAME_ROWS][FRAME_COLS]) {
    for (uint8_t r = 0; r < FRAME_ROWS; r++) {
        for (uint8_t c = 0; c < FRAME_COLS; c++) {
            char ch = frame[r][c];

            if ((unsigned char)ch < 32 || (unsigned char)ch > 126) {
                ch = '.';
            }

            sys_putchar_at(r, c, ch, COLOR_VIDEO);
        }
    }
}

static void clear_frame_buf(char frame[FRAME_ROWS][FRAME_COLS]) {
    for (uint8_t r = 0; r < FRAME_ROWS; r++) {
        for (uint8_t c = 0; c < FRAME_COLS; c++) {
            frame[r][c] = '.';
        }
    }
}

static int load_anim_file(uint32_t *out_size) {
    memset(anim_buf, 0, ANIM_BUF_SIZE);

    struct EXT2DriverRequest req = {
        .buf          = anim_buf,
        .name         = ANIM_FILENAME,
        .name_len     = ANIM_NAME_LEN,
        .parent_inode = 2,
        .buffer_size  = ANIM_BUF_SIZE,
        .is_folder    = false
    };

    int8_t rc = sys_read(&req);
    if (rc != 0) {
        return rc;
    }

    uint32_t len = 0;
    while (len < ANIM_BUF_SIZE && anim_buf[len] != 0) {
        len++;
    }
    *out_size = len;
    return 0;
}

static void play_from_buffer(const uint8_t *buf, uint32_t size) {
    char frame[FRAME_ROWS][FRAME_COLS];
    clear_frame_buf(frame);

    uint32_t idx = 0;
    uint8_t current_row = 0;
    bool has_frame = false;

    const uint32_t end = size;

    while (idx < end) {
        char line[FRAME_COLS + 1];
        uint32_t len = 0;

        while (idx < end && buf[idx] != '\n' && len < FRAME_COLS) {
            line[len++] = (char)buf[idx++];
        }

        while (idx < end && buf[idx] != '\n') {
            idx++;
        }
        if (idx < end && buf[idx] == '\n') {
            idx++;
        }

        line[len] = '\0';

        bool is_frame_sep =
            (len >= 11 &&
             line[0] == '-' && line[1] == '-' && line[2] == '-' &&
             line[3] == 'F' && line[4] == 'R' && line[5] == 'A' &&
             line[6] == 'M' && line[7] == 'E' &&
             line[8] == '-' && line[9] == '-' && line[10] == '-');

        if (is_frame_sep) {
            if (has_frame) {
                draw_frame(frame);
                sys_sleep(FRAME_DELAY_MS);

                if (check_ctrl_c()) {
                    return;
                }

                clear_frame_buf(frame);
                current_row = 0;
                has_frame = false;
            }
        } else {
            if (current_row < FRAME_ROWS) {
                for (uint8_t c = 0; c < FRAME_COLS; c++) {
                    char ch = (c < len) ? line[c] : ' ';
                    frame[current_row][c] = ch;
                }
                current_row++;
                has_frame = true;
            }
        }
    }

    if (has_frame) {
        draw_frame(frame);
        sys_sleep(FRAME_DELAY_MS);
    }
}

int badapple_play(void) {
    uint32_t anim_size = 0;

    sys_keyboard_activate();
    sys_clear();

    int rc = load_anim_file(&anim_size);
    if (rc != 0) {
        const char *msg = "badapple: failed to read /badapple.txt\n";
        sys_puts(msg, (uint32_t)strlen(msg), COLOR_VIDEO);
        return rc;
    }

    play_from_buffer(anim_buf, anim_size);

    return 0;
}

int main(void) {
    sys_puts("Badapple dayo\n", 15, COLOR_VIDEO);
    return badapple_play();
}

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEYBOARD_DATA_PORT        0x60
#define KEYBOARD_STATUS_PORT      0x64

/* Special scancode bytes */
#define EXTENDED_SCANCODE_BYTE    0xE0
#define BREAK_MASK                0x80

// Extended scancodes for non ASCII
#define EXT_SCANCODE_UP           0x48
#define EXT_SCANCODE_DOWN         0x50
#define EXT_SCANCODE_LEFT         0x4B
#define EXT_SCANCODE_RIGHT        0x4D
#define EXT_SCANCODE_PAGE_UP      0x49
#define EXT_SCANCODE_PAGE_DOWN    0x51

// Non-ASCII codes we push into the buffer
#define KEY_UP         0x80
#define KEY_DOWN       0x81
#define KEY_LEFT       0x82
#define KEY_RIGHT      0x83
#define KEY_PAGE_UP    0x84
#define KEY_PAGE_DOWN  0x85

#define KEYBOARD_BUFFER_SIZE   256

extern const char keyboard_scancode_1_to_ascii_map[256];

struct KeyboardState {
    bool kb_enabled;
    bool keyboard_input_on;
    uint8_t buffer_index;
    char keyboard_buffer;
} __attribute((packed));

void keyboard_state_activate(void);
void keyboard_state_deactivate(void);

void add_buffer(char c);
void get_keyboard_buffer(char *out_char);

void keyboard_isr(void);

void keyboard_process_scancode(uint8_t scancode);

#endif

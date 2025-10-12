#include "header/driver/keyboard.h"
#include "header/io/portio.h"
#include "header/cpu/interrupt.h" 

const char keyboard_scancode_1_to_ascii_map[256] = {
    0, 0x1B,  '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']', '\n',  0,   'a',  's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';','\'', '`',  0, '\\',  'z', 'x',   'c',  'v',
    'b', 'n', 'm', ',', '.', '/',  0,  '*',  0,  ' ',  0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,  '-',   0,    0,   0,  '+',    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,    0,    0,
};

static volatile char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t head = 0;
static volatile size_t tail = 0;
static volatile size_t count = 0;

static volatile bool kb_enabled = true;
static bool ext = false;
static bool shift = false;
static bool alt = false;
static bool ctrl = false;

static void keyboard_add_buffer(char c){
    if (count < KEYBOARD_BUFFER_SIZE) {
        keyboard_buffer[head] = c;
        head = (head + 1) % KEYBOARD_BUFFER_SIZE;
        count++;
    }
}

void get_keyboard_buffer(char *out) {
    if (!out) return;
    __asm__ volatile("cli");
    if (count > 0) {
        *out = keyboard_buffer[tail];
        tail = (tail + 1) % KEYBOARD_BUFFER_SIZE;
        count--;
        __asm__ volatile("sti");
    } else {
        __asm__ volatile("sti");
        *out = 0;
    }
}

void keyboard_state_activate(void) {
    kb_enabled = true;  
}
void keyboard_state_deactivate(void) {
    kb_enabled = false; 
}

// Converts base char on shift
static char keyboard_convert_shift(char base) {
    if (!shift) return base;

    switch (base) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default:
            if (base >= 'a' && base <= 'z') return (char)(base - 'a' + 'A');
            return base;
    }

}

void keyboard_process_scancode(uint8_t sc) {
    if (!kb_enabled) return;

    if (ext) {
        ext = false;
        switch (sc) {
            case EXT_SCANCODE_UP:        keyboard_add_buffer(KEY_UP); break;
            case EXT_SCANCODE_DOWN:      keyboard_add_buffer(KEY_DOWN); break;
            case EXT_SCANCODE_LEFT:      keyboard_add_buffer(KEY_LEFT); break;
            case EXT_SCANCODE_RIGHT:     keyboard_add_buffer(KEY_RIGHT); break;
            case EXT_SCANCODE_PAGE_UP:   keyboard_add_buffer(KEY_PAGE_UP); break;
            case EXT_SCANCODE_PAGE_DOWN: keyboard_add_buffer(KEY_PAGE_DOWN); break;
            default: break;
        }
        return;
    }

    if (sc == EXTENDED_SCANCODE_BYTE) { 
        ext = true; 
        return; 
    }

    bool is_break = (sc & BREAK_MASK) != 0;
    uint8_t make = sc & ~BREAK_MASK;

    if (make == 0x2A || make == 0x36) { //shift
        shift = !is_break; 
        return;
    }
    if (make == 0x1D) { //ctrl
        ctrl = !is_break;  
        return;
    }
    if (make == 0x38) { //alt
        alt  = !is_break;  
        return;
    }
    if (make == 0x0E) { // backspace
        if (!is_break) keyboard_add_buffer('\b');
        return;
    }
    if (make == 0x1C) { // enter
        if (!is_break) keyboard_add_buffer('\n');
        return;
    }

    if (is_break) return;

    char base = keyboard_scancode_1_to_ascii_map[make];
    if (base) {
        char out = keyboard_convert_shift(base);
        if (ctrl && (out == 'c' || out == 'C')) out = 3;
        keyboard_add_buffer(out);
    }
}

void keyboard_isr(void) {
    uint8_t sc = in(KEYBOARD_DATA_PORT);
    keyboard_process_scancode(sc);
    pic_ack(IRQ_KEYBOARD);
}

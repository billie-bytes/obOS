#include <stdint.h>
#include "header/stdlib/string.h"

#define COLOR_PROMPT_USER  0x0A
#define COLOR_PROMPT_SEP   0x07

char current_path[256] = "/";
// Syscall wrapper
static inline void syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("int $0x30" : : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

// Syscall wrappers for text output
static inline void sys_putchar(char c, uint8_t color) {
    syscall_do(5, (uint32_t)c, (uint32_t)color, 0);
}

static inline void sys_puts(const char *s, uint32_t len, uint8_t color) {
    syscall_do(6, (uint32_t)s, len, (uint32_t)color);
}

// Syscall 10: Exit
static inline void sys_exit(void) {
    syscall_do(10, 0, 0, 0);
}

// Syscall 18: Play tone
static inline void sys_speaker_play(uint32_t frequency) {
    syscall_do(18, frequency, 0, 0);
}

// Syscall 19: Stop speaker
static inline void sys_speaker_stop(void) {
    syscall_do(19, 0, 0, 0);
}

// Syscall 20: Beep
static inline void sys_speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    syscall_do(20, frequency, duration_ms, 0);
}

#define COLOR_TEXT 0x0F

// Musical note frequencies
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

static void print_prompt(void) {
    sys_puts("jOSh@OS-IF2230", 14, COLOR_PROMPT_USER);
    sys_puts(":", 1, COLOR_PROMPT_SEP);
    sys_puts(current_path, strlen(current_path), COLOR_PROMPT_USER);
    sys_puts("$ ", 2, COLOR_PROMPT_SEP);
}

int main(void) {
    // sys_puts("PC Speaker Test Program\n", 24, COLOR_TEXT);
    sys_puts("Playing beeps...\n", 17, COLOR_TEXT);
    
    // Play startup beep
    // sys_puts("1. Startup beep (440Hz, 200ms)\n", 32, COLOR_TEXT);
    sys_speaker_beep(NOTE_A4, 200);
    
    // Short delay between beeps (busy wait)
    for (volatile uint32_t i = 0; i < 1000000; i++);
    
    // Play scale
    // sys_puts("2. Playing scale C-D-E-F-G-A-B-C\n", 34, COLOR_TEXT);
    sys_speaker_beep(NOTE_C4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_D4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_E4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_F4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_G4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_A4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_B4, 300);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_C5, 300);
    for (volatile uint32_t i = 0; i < 1000000; i++);
    
    // Play simple melody (Three Blind Mice pattern)
    // sys_puts("3. Playing simple melody\n", 25, COLOR_TEXT);
    sys_speaker_beep(NOTE_E4, 300);
    for (volatile uint32_t i = 0; i < 300000; i++);
    sys_speaker_beep(NOTE_D4, 300);
    for (volatile uint32_t i = 0; i < 300000; i++);
    sys_speaker_beep(NOTE_C4, 500);
    for (volatile uint32_t i = 0; i < 500000; i++);
    
    sys_speaker_beep(NOTE_E4, 300);
    for (volatile uint32_t i = 0; i < 300000; i++);
    sys_speaker_beep(NOTE_D4, 300);
    for (volatile uint32_t i = 0; i < 300000; i++);
    sys_speaker_beep(NOTE_C4, 500);
    for (volatile uint32_t i = 0; i < 1000000; i++);
    
    // sys_puts("\nSpeaker test completed!\n", 24, COLOR_TEXT);
    // sys_puts("\n", 1, COLOR_TEXT);
    // sys_puts("Press any key to continue...\n", 29, COLOR_TEXT);
    print_prompt();
    sys_exit();
    while(1);
    return 0;
}

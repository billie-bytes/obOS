#include <stdint.h>
#include "lib/string.h"
#include "user/command/syscall.h"

#define COLOR_PROMPT_USER  0x0A
#define COLOR_PROMPT_SEP   0x07
#define COLOR_TEXT 0x0F

char current_path[256] = "/";

// Domain Specific Syscalls (Not exposed generally in syscall.h yet)
static inline void sys_speaker_play(uint32_t frequency) { syscall_do(18, frequency, 0, 0); }
static inline void sys_speaker_stop(void) { syscall_do(19, 0, 0, 0); }
static inline void sys_speaker_beep(uint32_t frequency, uint32_t duration_ms) { syscall_do(20, frequency, duration_ms, 0); }

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
    sys_puts("Playing beeps...\n", 17, COLOR_TEXT);
    
    // Play startup beep
    sys_speaker_beep(NOTE_A4, 200);
    
    for (volatile uint32_t i = 0; i < 1000000; i++);
    
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
    
    print_prompt();
    sys_exit();
    while(1);
    return 0;
}

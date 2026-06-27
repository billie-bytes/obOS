#include "kernel/speaker.h"
#include "kernel/portio.h"

static bool speaker_playing = false;

void speaker_init(void) {
    speaker_stop();
}

void speaker_play_tone(uint32_t frequency) {
    if (frequency == 0) {
        speaker_stop();
        return;
    }
    
    // Calculate divisor for PIT
    // PIT frequency = PIT_BASE_FREQUENCY / divisor
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;
    
    // Ensure divisor is within valid range (must fit in 16 bits)
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1) divisor = 1;
    
    // Set PIT channel 2 to square wave mode
    out(PIT_COMMAND_PORT, PIT_CHANNEL_2_CMD);
    
    // Send frequency divisor to PIT channel 2
    // Low byte first, then high byte
    out(PIT_CHANNEL_2_PORT, (uint8_t)(divisor & 0xFF));
    out(PIT_CHANNEL_2_PORT, (uint8_t)((divisor >> 8) & 0xFF));
    
    // Enable speaker and connect to PIT channel 2
    uint8_t speaker_ctrl = in(SPEAKER_PORT);
    speaker_ctrl |= (SPEAKER_ENABLE_BIT | SPEAKER_GATE_BIT);
    out(SPEAKER_PORT, speaker_ctrl);
    
    speaker_playing = true;
}

void speaker_stop(void) {
    // Disable speaker by clearing control bits
    uint8_t speaker_ctrl = in(SPEAKER_PORT);
    speaker_ctrl &= ~(SPEAKER_ENABLE_BIT | SPEAKER_GATE_BIT);
    out(SPEAKER_PORT, speaker_ctrl);
    
    speaker_playing = false;
}

void speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_play_tone(frequency);
    volatile uint32_t delay = duration_ms * 10000;
    while (delay--) {
        __asm__ volatile("nop");
    }
    
    speaker_stop();
}

bool speaker_is_playing(void) {
    return speaker_playing;
}

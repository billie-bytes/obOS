#ifndef _SPEAKER_H
#define _SPEAKER_H

#include <stdint.h>
#include <stdbool.h>

// PC Speaker ports
#define SPEAKER_PORT           0x61
#define PIT_COMMAND_PORT       0x43
#define PIT_CHANNEL_2_PORT     0x42

// Speaker control bits in port 0x61
#define SPEAKER_ENABLE_BIT     0x01  // Enable speaker
#define SPEAKER_GATE_BIT       0x02  // Enable PIT channel 2 gate

// PIT channel 2 command
#define PIT_CHANNEL_2_CMD      0xB6  // Channel 2, Access mode: lobyte/hibyte, Mode 3 (square wave)

// PIT base frequency (Hz)
#define PIT_BASE_FREQUENCY     1193180

// Common musical notes (frequencies in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

/**
 * Initialize PC Speaker driver
 */
void speaker_init(void);

/**
 * Play a tone at specified frequency
 * 
 * @param frequency Frequency in Hz (e.g., 440 for A4)
 */
void speaker_play_tone(uint32_t frequency);

/**
 * Stop the speaker (turn off sound)
 */
void speaker_stop(void);

/**
 * Play a beep sound
 * 
 * @param frequency Frequency in Hz
 * @param duration_ms Duration in milliseconds (approximate, uses busy wait)
 */
void speaker_beep(uint32_t frequency, uint32_t duration_ms);

/**
 * Check if speaker is currently playing
 * 
 * @return true if speaker is on, false otherwise
 */
bool speaker_is_playing(void);

#endif

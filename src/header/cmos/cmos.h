#ifndef _CMOS_H
#define _CMOS_H
// referensi https://github.com/MuhamadAjiW/Chuu-Kawaii-OS
#include <stdint.h>
#include <stdbool.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define TIMEZONE 7

/**
 * Struct to store time information from CMOS
 * Should be self explanatory
 *
 */
struct cmos_reader
{
  uint8_t century;
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint16_t year;
};
typedef struct cmos_reader cmos_reader;

/**
 * Set 24 hour flag in CMOS via porting
 *
 */
void initialize_cmos();

/**
 * Check busy port in CMOS
 *
 */
bool update_in_progress();

/**
 * Get values of CMOS registers
 *
 * @param reg requested register index
 *
 * @return value of requested register
 */
uint8_t get_reg(int reg);

/**
 * Update static rtc values
 *
 */
void read_rtc();

/**
 * Return values of stored static rtc values
 *
 * @return static rtc values
 */
cmos_reader get_cmos_data();

#endif
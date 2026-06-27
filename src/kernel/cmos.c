#include "kernel/cmos.h"
#include "kernel/portio.h"

// referensi https://github.com/MuhamadAjiW/Chuu-Kawaii-OS
// referensi https://wiki.osdev.org/CMOS

/**
 * Static struct to store time information from CMOS
 * Reading should always be done to this variable
 *
 */
static cmos_reader cmos_data = {
    .century = 0,
    .second = 0,
    .minute = 0,
    .hour = 0,
    .day = 0,
    .month = 0,
    .year = 0};

void initialize_cmos()
{
  // intinya ngeset mode 24 jam, itu doang
  out(CMOS_ADDR, 0x0b);
  uint8_t former = in(CMOS_DATA);
  out(CMOS_DATA, former | 0x02 | 0x04);
}

bool update_in_progress()
{
  // cek running
  out(CMOS_ADDR, 0x0A);
  return (in(CMOS_DATA) & 0x80);
}

uint8_t get_reg(int reg)
{
  // cek running
  out(CMOS_ADDR, reg);
  return in(CMOS_DATA);
}

void read_rtc()
{
  initialize_cmos();

  while (update_in_progress())
  {
  }; // nunggu clear
  cmos_data.second = get_reg(0x00);
  cmos_data.minute = get_reg(0x02);
  cmos_data.hour = (get_reg(0x04) + TIMEZONE) % 24;
  cmos_data.day = get_reg(0x07);
  cmos_data.month = get_reg(0x08);
  cmos_data.year = get_reg(0x09);
  cmos_data.century = get_reg(0x32);
}

cmos_reader get_cmos_data()
{
  read_rtc();
  return cmos_data;
}
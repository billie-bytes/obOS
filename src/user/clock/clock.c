#include <stdint.h>
#include "../command/syscall.h"

#define COLOR_CLOCK 0x0F


uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void print_two_digits(uint8_t row, uint8_t col, uint8_t value) {
    uint8_t tens = value / 10;
    uint8_t ones = value % 10;
    sys_putchar_at(row, col, '0' + tens, COLOR_CLOCK);
    sys_putchar_at(row, col + 1, '0' + ones, COLOR_CLOCK);
}

int main(void) {
    struct cmos_reader time;
    uint8_t row = 24;
    uint8_t col = 71;
    
    while (1) {
        sys_get_cmos(&time);
        uint8_t hour = bcd_to_binary(time.hour);
        uint8_t minute = bcd_to_binary(time.minute);
        uint8_t second = bcd_to_binary(time.second);
        
        print_two_digits(row, col, hour);
        sys_putchar_at(row, col + 2, ':', COLOR_CLOCK);
        print_two_digits(row, col + 3, minute);
        sys_putchar_at(row, col + 5, ':', COLOR_CLOCK);
        print_two_digits(row, col + 6, second);
        
        sys_sleep(1000);
    }
    
    return 0;
}

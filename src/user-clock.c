#include <stdint.h>

#define COLOR_CLOCK 0x0F

struct cmos_reader {
    uint8_t century;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

static inline void sys_get_cmos(struct cmos_reader *buf) {
    syscall_do(14, (uint32_t)buf, 0, 0);
}

static inline void sys_putchar_at(uint8_t row, uint8_t col, char c, uint8_t color) {
    __asm__ volatile("mov %0, %%ebx" : : "r"((uint32_t)row));
    __asm__ volatile("mov %0, %%ecx" : : "r"((uint32_t)col));
    __asm__ volatile("mov %0, %%edx" : : "r"((uint32_t)(uint8_t)c));
    __asm__ volatile("mov %0, %%edi" : : "r"((uint32_t)color));
    __asm__ volatile("mov $15, %%eax" : : );
    __asm__ volatile("int $0x30");
}

static inline void sys_sleep(uint32_t ms) {
    syscall_do(9, ms, 0, 0);
}

// Convert BCD to binary
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
    
    // Clock position: bottom right corner
    // Assuming 25 rows, 80 cols screen
    // Format: HH:MM:SS (8 characters)
    uint8_t row = 24;  // Bottom row
    uint8_t col = 71;  // 80 - 8 - 1 = 71
    
    while (1) {
        sys_get_cmos(&time);
        
        // Convert BCD to binary if needed
        uint8_t hour = bcd_to_binary(time.hour);
        uint8_t minute = bcd_to_binary(time.minute);
        uint8_t second = bcd_to_binary(time.second);
        
        // Print time in format HH:MM:SS
        print_two_digits(row, col, hour);
        sys_putchar_at(row, col + 2, ':', COLOR_CLOCK);
        print_two_digits(row, col + 3, minute);
        sys_putchar_at(row, col + 5, ':', COLOR_CLOCK);
        print_two_digits(row, col + 6, second);
        
        // Sleep for 1 second (1000ms)
        sys_sleep(1000);
    }
    
    return 0;
}

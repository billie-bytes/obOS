#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>

/* IDT has a fixed 256 entries on x86 */
#define IDT_MAX_ENTRY_COUNT 256
#define ISR_STUB_TABLE_LIMIT 64

/* One IDT entry */
struct IDTGate {
    // First 32-bit (Bit 0 to 31)
    uint16_t offset_low;
    uint16_t segment;
    uint8_t _reserved   : 5;
    uint8_t _r_bit_1    : 3;
    uint8_t _r_bit_2    : 3;
    uint8_t gate_32     : 1;
    uint8_t _r_bit_3    : 1;
    uint8_t dpl         : 2;
    uint8_t valid_bit   : 1;
    uint16_t offset_high;
} __attribute__((packed));

struct InterruptDescriptorTable {
    struct IDTGate table[IDT_MAX_ENTRY_COUNT];
} __attribute__((packed));

struct IDTR {
    uint16_t size;
    struct IDTGate* address;
} __attribute__((packed));

#define INTERRUPT_GATE_R_BIT_1 0b000
#define INTERRUPT_GATE_R_BIT_2 0b110
#define INTERRUPT_GATE_R_BIT_3 0b000

extern struct InterruptDescriptorTable interrupt_descriptor_table;
extern struct IDTR _idt_idtr;

void initialize_idt(void);
void set_interrupt_gate(uint8_t, void*, uint16_t, uint8_t);

#endif

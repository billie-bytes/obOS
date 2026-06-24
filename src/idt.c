#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"

extern void* isr_stub_table[64];

struct InterruptDescriptorTable interrupt_descriptor_table = {0};

struct IDTR _idt_idtr = {
    .size    = sizeof(interrupt_descriptor_table) - 1,
    .address = &interrupt_descriptor_table.table[0],
};

void initialize_idt(void) {
    for (uint16_t i = 0; i < 64; i++){
        set_interrupt_gate( (uint8_t) i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }
    // Allow user mode to invoke syscall via int 0x30 (DPL=3)
    set_interrupt_gate(0x30, isr_stub_table[0x30], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 3);
    /* 
     * Iterate all isr_stub_table,
     * Set all IDT entry with set_interrupt_gate()
     * with following values:
     * Vector: i
     * Handler Address: isr_stub_table[i]
     * Segment: GDT_KERNEL_CODE_SEGMENT_SELECTOR
     * Privilege: 0
     */
    __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
    __asm__ volatile("sti");
}

void set_interrupt_gate(
    uint8_t  int_vector, 
    void     *handler_address, 
    uint16_t gdt_seg_selector, 
    uint8_t  privilege
) {
    struct IDTGate *idt_int_gate = &interrupt_descriptor_table.table[int_vector];
    uint32_t addr = (uint32_t) handler_address;

    /* handler offset, privilege, segment */
    idt_int_gate->offset_low  = (uint16_t) (addr & 0xFFFF);
    idt_int_gate->offset_high = (uint16_t) ((addr >> 16) & 0xFFFF);
    idt_int_gate->segment     = gdt_seg_selector;
    idt_int_gate->dpl         = privilege & 0x3;

    // Target system 32-bit and flag this as valid interrupt gate
    idt_int_gate->_r_bit_1    = INTERRUPT_GATE_R_BIT_1;
    idt_int_gate->_r_bit_2    = INTERRUPT_GATE_R_BIT_2;
    idt_int_gate->_r_bit_3    = INTERRUPT_GATE_R_BIT_3;
    idt_int_gate->gate_32     = 1;
    idt_int_gate->valid_bit   = 1;
}

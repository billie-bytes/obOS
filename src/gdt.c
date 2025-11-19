#include "header/cpu/gdt.h"
#include "header/cpu/interrupt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {
            /* Null Descriptor */
            .segment_low = 0,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0,
            .non_system = 0,
            .privilege_bit = 0,
            .present_bit = 0,
            .limit_bit = 0,
            .available_bit = 0,
            .long_bit = 0,
            .default_operation_bit = 0,
            .granularity_bit = 0,
            .base_high = 0
        },
        { 
            /* Kernel Code Segment */
            .segment_low = 0xFFFF,
            .base_low = 0,

            .base_mid = 0,
            .type_bit = 0xA,
            .non_system = 1,
            .privilege_bit = 00,
            .present_bit = 1,

            .limit_bit = 0xF,

            .available_bit = 0,
            .long_bit = 0,
            .default_operation_bit = 1,
            .granularity_bit = 1,

            .base_high = 0
        },
        {
            /* Kernel Data Segment */
            .segment_low = 0xFFFF,
            .base_low = 0x0,

            .base_mid = 0,
            .type_bit = 0x2,
            .non_system = 1,
            .privilege_bit = 00,
            .present_bit = 1,

            .limit_bit = 0xF,

            .available_bit = 0,
            .long_bit = 0,
            .default_operation_bit = 1,
            .granularity_bit = 1,

            .base_high = 0
        },
        {
            /* User   Code Descriptor */
            .segment_low = 0xFFFF,
            .base_low = 0,

            .base_mid = 0,
            .type_bit = 0xA,
            .non_system = 1,
            .privilege_bit = 0x3,
            .present_bit = 1,

            .limit_bit = 0xF,

            .available_bit = 0,
            .long_bit = 0,
            .default_operation_bit = 1,
            .granularity_bit = 1,

            .base_high = 0

        },
        {
            /* User   Data Descriptor */
            .segment_low = 0xFFFF,
            .base_low = 0x0,

            .base_mid = 0,
            .type_bit = 0x2,
            .non_system = 1,            // S
            .privilege_bit = 0x3,       // DPL
            .present_bit = 1,           // P

            .limit_bit = 0xF,

            .available_bit = 0,         //
            .long_bit = 0,              // L
            .default_operation_bit = 1, // D/B
            .granularity_bit = 1,       // G

            .base_high = 0

        },
        {
            /* TSS Descriptor */
            .segment_low                  = sizeof(struct TSSEntry),
            .base_low                     = 0,
            
            .base_mid                     = 0,
            .type_bit                     = 0x9,
            .non_system                   = 0,    // S bit
            .privilege_bit                = 0,    // DPL
            .present_bit                  = 1,    // P bit

            .limit_bit                    = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            
            .default_operation_bit        = 1,    // D/B bit
            .long_bit                     = 0,    // L bit
            .granularity_bit              = 0,    // G bit

            .base_high                    = 0,
        },
        {0}
    }
};

/**
 * _gdt_gdtr, predefined system GDTR. 
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    .size = sizeof(struct GlobalDescriptorTable) - 1,
    .address = &(global_descriptor_table)
};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low  = base & 0xFFFF;
}


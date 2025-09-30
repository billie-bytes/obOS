#include "header/cpu/gdt.h"

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
        }
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

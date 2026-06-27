ENTRY(_start)

SECTIONS {
    . = 0x00000000;
    
    .text ALIGN(4K): {
        *crt0-cmd.o(.text)
        *(.text)
        *(.rodata)
    }

    .data ALIGN(4K): {
        *(.data)
    }

    .bss ALIGN(4K): {
        *(COMMON)
        *(.bss)
    }
    
    _linker_user_program_end = .;
    ASSERT((_linker_user_program_end <= 4 * 1024 * 1024), "Error: Command program too large")
    
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}

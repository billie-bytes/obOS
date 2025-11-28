#include <stdint.h>
#include "header/io/portio.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/scheduler/scheduler.h"

struct TSSEntry _interrupt_tss_entry = {
    .ss0  = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void syscall(struct InterruptFrame frame);

void io_wait(void) {
    out(0x80, 0);
}

void pic_ack(uint8_t irq) {
    if (irq >= 8) out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void) {
    // Starts the initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); 
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
    io_wait();
    out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
    io_wait();
    out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case 0x30:
            syscall(frame);
            break;
        case (PIC1_OFFSET + IRQ_KEYBOARD):
            keyboard_isr();
            break;
        case (PIC1_OFFSET + IRQ_TIMER):
            timer_isr(frame);
            break;
        default:
            break;
    }
}

void activate_keyboard_interrupt(void) {
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

void set_tss_kernel_current_stack(void) {
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile ("mov %%ebp, %0": "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8; 
}

void syscall(struct InterruptFrame frame) {
    switch (frame.cpu.general.eax) {
        case 0:
        /* read() */
            if (frame.cpu.general.ecx) {
                *((int8_t*) frame.cpu.general.ecx) = read(
                    *(struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
        case 1:
        /* read_directory() */
            if (frame.cpu.general.ecx) {
                *((int8_t*) frame.cpu.general.ecx) = read_directory(
                    (struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
        case 2:
        /* write() */
            if (frame.cpu.general.ecx) {
                *((int8_t*) frame.cpu.general.ecx) = write(
                    *(struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
        case 3:
        /* delete() */
            if (frame.cpu.general.ecx) {
                *((int8_t*) frame.cpu.general.ecx) = delete(
                    *(struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
        case 4:
        /* Keyboard I/O getchar() */
            get_keyboard_buffer((char*) frame.cpu.general.ebx);
            break;
        case 5:
        /* putchar() */
            putchar(
                (char)(frame.cpu.general.ebx & 0xFF),
                (uint8_t)(frame.cpu.general.ecx & 0x0F)
            );
            break;
        case 6:
        /* puts() */
            if (frame.cpu.general.ebx) {
                uint32_t cnt = frame.cpu.general.ecx;
                if (cnt > 2000) cnt = 2000; /* basic clamp */
                puts(
                    (char*) frame.cpu.general.ebx, 
                    cnt, 
                    (uint8_t)(frame.cpu.general.edx & 0x0F)
                );
            }
            break;
        case 7: 
            keyboard_state_activate();
            break;
    }
}

void activate_timer_interrupt(void) {
    __asm__ volatile("cli");
    // Setup how often PIT fire
    uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) (pit_timer_counter_to_fire & 0xFF));
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) ((pit_timer_counter_to_fire >> 8) & 0xFF));

    // Activate the interrupt
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
    // __asm__ volatile("sti");

}






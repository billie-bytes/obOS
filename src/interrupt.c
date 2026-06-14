#include <stdint.h>
#include "header/io/portio.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/scheduler/scheduler.h"
#include "header/process/process.h"
#include "header/cmos/cmos.h"
#include "header/driver/speaker.h"

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

extern uint64_t scheduler_get_ticks(void);

static inline uint32_t ms_to_ticks(uint32_t ms) {
    uint32_t prod = ms * (uint32_t)PIT_TIMER_FREQUENCY;
    return (prod + 999u) / 1000u;
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
        case 8:
        /* Clear screen */
            framebuffer_clear();
            break;

        case 9:
        /* Sleep */
            {
                struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();
                if (pcb != NULL) {
                    uint64_t now = scheduler_get_ticks();
                    uint32_t ms = (uint32_t)frame.cpu.general.ebx;
                    uint32_t add_ticks = ms_to_ticks(ms);
                    pcb->metadata.wake_tick = now + (uint64_t)add_ticks;
                    pcb->metadata.state = PROCESS_SLEEPING;
                    scheduler_switch_to_next_process();
                }
            }
            break;
        case 10:
        /* Process exit - terminate current process */
            struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();
                if (pcb != NULL){
                    pcb->metadata.state = PROCESS_TERMINATED;
                    process_manager_state._process_used[pcb->metadata.pid] = false;
                    process_manager_state.active_process_count--;
                    scheduler_switch_to_next_process();
                }
            break;
        case 16:
        /* Set cursor position: ebx=row, ecx=col */
            framebuffer_set_cursor((uint8_t)frame.cpu.general.ebx, (uint8_t)frame.cpu.general.ecx);
            break;
        case 17:
        /* Get cursor position: ebx=ptr_row, ecx=ptr_col */
            if (frame.cpu.general.ebx && frame.cpu.general.ecx) {
                
            }
            break;
        case 11:
        /* Create new user process */
            struct EXT2DriverRequest *req = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
            if (req == NULL) {
                frame.cpu.general.eax = -1;
            } else {
                frame.cpu.general.eax = process_create_user_process(*req);
            }
            break;
        case 12:
        /* Destroy process by pid */    
            frame.cpu.general.eax = process_destroy(frame.cpu.general.ebx) ? 0 : -1;
            break;
        case 13:
        /* Get process info */
            // ebx = pointer to ProcessInfo
            // ecx = size of ProcessInfo
            // eax = number of process info written
            frame.cpu.general.eax = get_process_info(
                (ProcessInfo *)frame.cpu.general.ebx,
                frame.cpu.general.ecx);
            break;
        case 14:
        /* Get CMOS time data */
            // ebx = pointer to cmos_reader struct
            cmos_reader *cmos_buf = (cmos_reader *)frame.cpu.general.ebx;
            *cmos_buf = get_cmos_data();
            break;
        case 15:
        /* Put char at specific position */
            // ebx = row, ecx = col, edx = char, edi = color
            framebuffer_write((uint8_t)frame.cpu.general.ebx, (uint8_t)frame.cpu.general.ecx, 
                             (char)frame.cpu.general.edx, (uint8_t)frame.cpu.index.edi, 0);
            break;
        case 18:
        /* Play tone on PC Speaker */
            // ebx = frequency in Hz
            speaker_play_tone(frame.cpu.general.ebx);
            break;
        case 19:
        /* Stop PC Speaker */
            speaker_stop();
            break;
        case 20:
        /* Beep on PC Speaker */
            // ebx = frequency in Hz
            // ecx = duration in milliseconds
            speaker_beep(frame.cpu.general.ebx, frame.cpu.general.ecx);
            break;
        case 21:
        /* System shutdown (attempt to power off QEMU/Bochs) */
            __asm__ volatile ("cli");
            // Try well-known ports for QEMU/Bochs shutdown
            out16(0xB004, 0x2000);  // Bochs/QEMU old
            out16(0x0604, 0x2000);  // QEMU (ISA PM)
            out16(0x4004, 0x3400);  // VirtualBox fallback
            // If still running, halt forever
            for (;;) { __asm__ volatile ("hlt"); }
            break;
        case 22:
        /* Get current working directory */
            // ebx = buffer pointer, ecx = buffer size
            // Returns: eax = current_directory_inode
            // For now, always return root inode (2)
            // TODO: implement per-process cwd in PCB
            frame.cpu.general.eax = 2; // Root inode
            if (frame.cpu.general.ebx && frame.cpu.general.ecx > 0) {
                char *buf = (char*)frame.cpu.general.ebx;
                buf[0] = '/';
                buf[1] = '\0';
            }
            break;
        case 23:
        /* Set current working directory */
            // ebx = inode, ecx = path string
            // TODO: implement per-process cwd in PCB
            // For now, just return success
            frame.cpu.general.eax = 0;
            break;
        case 24:
        /* ebx = parent_inode, ecx = name string pointer, edx = out_type pointer */
            frame.cpu.general.eax = fs_stat(
                frame.cpu.general.ebx,
                (char*)frame.cpu.general.ecx,
                (uint8_t*)frame.cpu.general.edx
            );
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
    __asm__ volatile("sti");

}


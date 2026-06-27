#include <stdint.h>
#include "kernel/portio.h"
#include "kernel/interrupt.h"
#include "kernel/keyboard.h"
#include "kernel/gdt.h"
#include "kernel/ext2.h"
#include "kernel/framebuffer.h"
#include "kernel/scheduler.h"
#include "kernel/process.h"
#include "kernel/cmos.h"
#include "kernel/speaker.h"

struct TSSEntry _interrupt_tss_entry = {
    .ss0  = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};
uint32_t cwd_inode = 2;
static char global_cwd_path[256] = "/";
void syscall(struct InterruptFrame *frame);
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
            syscall(&frame);
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

bool kernel_resolve_path(const char* target, uint32_t* out_inode, char* out_path) {
    char path_copy[256];
    strncpy(path_copy, target, 255);
    path_copy[255] = '\0';

    uint32_t current_inode = (path_copy[0] == '/') ? 2 : cwd_inode;
    char new_path[256];
    
    if (path_copy[0] == '/') strcpy(new_path, "/");
    else strcpy(new_path, global_cwd_path);

    char* p = path_copy;
    while (*p == '/') p++; // Skip leading slashes

    while (*p) {
        char* end = p;
        while (*end && *end != '/') end++;
        bool last = (*end == '\0');
        *end = '\0';

        if (strlen(p) > 0 && strcmp(p, ".") != 0) {
            if (strcmp(p, "..") == 0) {
                // Traverse Up
                uint8_t type;
                uint32_t up_inode = fs_stat(current_inode, "..", &type);
                if (!up_inode) return false;
                current_inode = up_inode;

                // Strip last directory from string
                size_t len = strlen(new_path);
                if (len > 1) {
                    if (new_path[len-1] == '/') len--;
                    while (len > 1 && new_path[len-1] != '/') len--;
                    new_path[len-1] = '\0';
                    if (len == 1) new_path[1] = '\0';
                }
            } else {
                // Traverse Down
                uint8_t type;
                uint32_t next_inode = fs_stat(current_inode, p, &type);
                if (!next_inode) return false;
                current_inode = next_inode;

                if (strcmp(new_path, "/") != 0) strcat(new_path, "/");
                strcat(new_path, p);
            }
        }
        
        if (last) break;
        p = end + 1;
        while (*p == '/') p++; // Skip multiple slashes
    }

    *out_inode = current_inode;
    strcpy(out_path, new_path);
    return true;
}

void syscall(struct InterruptFrame *frame) {
    switch (frame->cpu.general.eax) {
        case 0:
        /* read() */
            if (frame->cpu.general.ecx) {
                *((int8_t*) frame->cpu.general.ecx) = read(
                    *(struct EXT2DriverRequest*) frame->cpu.general.ebx
                );
            }
            break;
        case 1:
        /* read_directory() */
            if (frame->cpu.general.ecx) {
                *((int8_t*) frame->cpu.general.ecx) = read_directory(
                    (struct EXT2DriverRequest*) frame->cpu.general.ebx
                );
            }
            break;
        case 2:
        /* write() */
            if (frame->cpu.general.ecx) {
                *((int8_t*) frame->cpu.general.ecx) = write(
                    *(struct EXT2DriverRequest*) frame->cpu.general.ebx
                );
            }
            break;
        case 3:
        /* delete() */
            if (frame->cpu.general.ecx) {
                *((int8_t*) frame->cpu.general.ecx) = delete(
                    *(struct EXT2DriverRequest*) frame->cpu.general.ebx
                );
            }
            break;
        case 4:
        /* Keyboard I/O getchar() */
            get_keyboard_buffer((char*) frame->cpu.general.ebx);
            break;
        case 5:
        /* putchar() */
            putchar(
                (char)(frame->cpu.general.ebx & 0xFF),
                (uint8_t)(frame->cpu.general.ecx & 0x0F)
            );
            break;
        case 6:
        /* puts() */
            if (frame->cpu.general.ebx) {
                uint32_t cnt = frame->cpu.general.ecx;
                if (cnt > 2000) cnt = 2000; /* basic clamp */
                puts(
                    (char*) frame->cpu.general.ebx, 
                    cnt, 
                    (uint8_t)(frame->cpu.general.edx & 0x0F)
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
                    uint32_t ms = (uint32_t)frame->cpu.general.ebx;
                    uint32_t add_ticks = ms_to_ticks(ms);
                    pcb->metadata.wake_tick = now + (uint64_t)add_ticks;
                    pcb->metadata.state = PROCESS_SLEEPING;
                    scheduler_switch_to_next_process();
                }
            }
            break;
        case 10:
        /* Process exit - terminate current process */
            {
                struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();
                if (pcb != NULL){
                    pcb->metadata.state = PROCESS_TERMINATED;
                    process_manager_state._process_used[pcb->metadata.pid] = false;
                    process_manager_state.active_process_count--;
                    scheduler_switch_to_next_process();
                }
            }
            break;
        case 11:
        /* Create new user process */
            {
                struct EXT2ProgramRequest *req = (struct EXT2ProgramRequest *)frame->cpu.general.ebx;
                if (req == NULL) {
                    frame->cpu.general.eax = -1;
                } else {
                    frame->cpu.general.eax = process_create_user_process(*req);
                }
            }
            break;
        case 12:
        /* Destroy process by pid */    
            frame->cpu.general.eax = process_destroy(frame->cpu.general.ebx) ? 0 : -1;
            break;
        case 13:
        /* Get process info */
            frame->cpu.general.eax = get_process_info(
                (ProcessInfo *)frame->cpu.general.ebx,
                frame->cpu.general.ecx);
            break;
        case 14:
        /* Get CMOS time data */
            {
                cmos_reader *cmos_buf = (cmos_reader *)frame->cpu.general.ebx;
                *cmos_buf = get_cmos_data();
            }
            break;
        case 15:
        /* Put char at specific position */
            framebuffer_write((uint8_t)frame->cpu.general.ebx, (uint8_t)frame->cpu.general.ecx, 
                             (char)frame->cpu.general.edx, (uint8_t)frame->cpu.index.edi, 0);
            break;
        case 16:
        /* Set cursor position: ebx=row, ecx=col */
            framebuffer_set_cursor((uint8_t)frame->cpu.general.ebx, (uint8_t)frame->cpu.general.ecx);
            break;
        case 17:
        /* Get cursor position */
            break;
        case 18:
        /* Play tone on PC Speaker */
            speaker_play_tone(frame->cpu.general.ebx);
            break;
        case 19:
        /* Stop PC Speaker */
            speaker_stop();
            break;
        case 20:
        /* Beep on PC Speaker */
            speaker_beep(frame->cpu.general.ebx, frame->cpu.general.ecx);
            break;
        case 21:
        /* System shutdown */
            __asm__ volatile ("cli");
            out16(0xB004, 0x2000); 
            out16(0x0604, 0x2000); 
            out16(0x4004, 0x3400); 
            for (;;) { __asm__ volatile ("hlt"); }
            break;

        // CHANGE THESE WHEN IMPLEMENTING MULTITASKING
        case 22:
        /* POSIX getcwd: char *getcwd(char *buf, size_t size); */
            if (frame->cpu.general.ebx && frame->cpu.general.ecx > 0) {
                char *buf = (char*)frame->cpu.general.ebx;
                uint32_t size = frame->cpu.general.ecx;
                uint32_t path_len = strlen(global_cwd_path);
                
                if (path_len + 1 > size) {
                    frame->cpu.general.eax = -1; // ERANGE
                } else {
                    strcpy(buf, global_cwd_path);
                    frame->cpu.general.eax = path_len; // Success
                }
            } else {
                frame->cpu.general.eax = -1; // EINVAL
            }
            break;

        case 23:
        /* POSIX chdir: int chdir(const char *path); */
            {
                const char* target_path = (char*)frame->cpu.general.ebx;
                uint32_t new_inode;
                char new_string_path[256];

                if (kernel_resolve_path(target_path, &new_inode, new_string_path)) {
                    uint8_t type = 0;
                    // Confirm it is actually a directory
                    if (fs_stat(new_inode, ".", &type) && type == EXT2_FT_DIR) {
                        cwd_inode = new_inode;
                        strcpy(global_cwd_path, new_string_path);
                        frame->cpu.general.eax = 0; // Success
                    } else {
                        frame->cpu.general.eax = -1; // ENOTDIR
                    }
                } else {
                    frame->cpu.general.eax = -1; // ENOENT
                }
            }
            break;

        case 24:
        /* POSIX stat (Simplified): Get inode by full/relative path string */
            {
                const char* target_path = (char*)frame->cpu.general.ebx;
                uint8_t* out_type = (uint8_t*)frame->cpu.general.ecx;
                uint32_t target_inode;
                char dummy_path[256];
                
                if (kernel_resolve_path(target_path, &target_inode, dummy_path)) {
                    if (out_type) {
                        fs_stat(target_inode, ".", out_type);
                    }
                    frame->cpu.general.eax = target_inode; // Returns the valid inode
                } else {
                    frame->cpu.general.eax = 0; // File not found
                }
            }
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


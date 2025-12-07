#include "header/scheduler/scheduler.h"
#include "header/cpu/interrupt.h"
#include "header/process/process.h"
#include "header/stdlib/string.h"
#include "header/text/framebuffer.h"
#include "header/memory/paging.h"

void scheduler_init(void) {
    activate_timer_interrupt();
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock *current_pcb = process_get_current_running_pcb_pointer();
    if (current_pcb != NULL) {
        current_pcb->context = ctx;
    }
}

void timer_isr(struct InterruptFrame frame) {
    // Acknowledge PIC first
    pic_ack(IRQ_TIMER);
    
    // For single process, simply return without context switching
    // This prevents corruption from switching to the same process
    if (process_manager_state.active_process_count <= 1) {
        return; // Just return, let the process continue
    }
    
    // Build complete context from interrupt frame
    struct Context ctx = {
        .cpu = frame.cpu,
        .eip = frame.int_stack.eip,
        .cs = frame.int_stack.cs,
        .eflags = frame.int_stack.eflags,
        // Use interprivilege stack values if present (when interrupt from user mode)
        .esp = (frame.int_stack.esp_privilege_change) ? frame.int_stack.esp_privilege_change : frame.cpu.stack.esp,
        .ss = (frame.int_stack.ss_privilege_change) ? frame.int_stack.ss_privilege_change : (0x20 | 0x3),
        .page_directory_virtual_addr = paging_get_current_page_directory_addr()
    };
    
    // Save context to current running process
    scheduler_save_context_to_current_running_pcb(ctx);
    
    // Perform scheduling: switch to next process
    // This function is noreturn - it will iret to the next process
    scheduler_switch_to_next_process();
}

__attribute__((noreturn)) void scheduler_switch_to_next_process(void) {
    int process_index = -1;
    struct ProcessControlBlock *current_pcb = process_get_current_running_pcb_pointer();

    if (current_pcb != NULL) {
        process_index = current_pcb->metadata.pid % PROCESS_COUNT_MAX;
        current_pcb->metadata.state = PROCESS_READY;
    }
    
    // Round-robin scheduling: find next ready process
    do {
        process_index = (process_index + 1) % PROCESS_COUNT_MAX;
        // Check if process slot is used AND state is READY
        if (process_manager_state._process_used[process_index] && 
            _process_list[process_index].metadata.state == PROCESS_READY) {
            struct ProcessControlBlock *next_pcb = &_process_list[process_index];
            next_pcb->metadata.state = PROCESS_RUNNING;
            
            // Update TSS.esp0 for kernel stack when handling interrupts from user mode
            set_tss_kernel_current_stack();
            
            // Switch to process's virtual address space
            paging_use_page_directory(next_pcb->context.page_directory_virtual_addr);
            
            // Perform context switch
            process_context_switch(next_pcb->context);
        }
    } while (1);
}
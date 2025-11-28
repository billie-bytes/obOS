#include "header/scheduler/scheduler.h"
#include "header/cpu/interrupt.h"
#include "header/process/process.h"
#include "header/stdlib/string.h"

struct ProcessControlBlock new_pcb = {
};

struct ProcessControlBlock curent_pcb = {
};

uint32_t current_process_idx = 0;
uint32_t next_process_idx = 0;

void scheduler_init(void) {
    activate_timer_interrupt();
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock *current_pcb = process_get_current_running_pcb_pointer();
    memcpy(&(*current_pcb).context, &ctx, sizeof(struct Context));
}

__attribute__((noreturn)) void scheduler_switch_to_next_process(void){
    _process_list[current_process_idx].metadata.state = PROCESS_READY;

    current_process_idx = next_process_idx;

    uint32_t i = current_process_idx + 1;

    while(i != current_process_idx){    
        if(process_manager_state._process_used[i] && _process_list[i].metadata.state == PROCESS_READY){
            next_process_idx = i;
            break;
        }
        i++;
        if(i >= PROCESS_COUNT_MAX){
            i = 0;
        }
    }

    if(i == current_process_idx){
        next_process_idx = current_process_idx;
    } else {
        next_process_idx = i;
    }
    // change state
    _process_list[current_process_idx].metadata.state = PROCESS_RUNNING;

    /* Update TSS.esp0 so kernel has a valid stack when handling interrupts
     * that occur while the next process is running. */
    set_tss_kernel_current_stack();

    paging_use_page_directory(_process_list[current_process_idx].context.page_directory_virtual_addr);

    /* Acknowledge timer IRQ using IRQ number (pic_ack expects IRQ, not vector). */
    pic_ack(IRQ_TIMER);

    process_context_switch(_process_list[current_process_idx].context);
}
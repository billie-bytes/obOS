#include "header/scheduler/scheduler.h"
#include "header/cpu/interrupt.h"
#include "header/process/process.h"
#include "header/stdlib/string.h"
#include "header/text/framebuffer.h"

struct ProcessControlBlock new_pcb = {
};

struct ProcessControlBlock curent_pcb = {
};

uint32_t current_process_idx = 0;
uint32_t next_process_idx = 0;

/* Runtime toggle for diagnostics: when true, scheduler updates state but
 * does not perform the assembly iret-based context switch. Prefer this
 * runtime flag over preprocessor conditionals to keep source files tidy. */
static bool scheduler_skip_context_switch = false;

void scheduler_set_skip_context_switch(bool v) {
    scheduler_skip_context_switch = v;
}

void scheduler_init(void) {
    activate_timer_interrupt();
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock *current_pcb = process_get_current_running_pcb_pointer();
    memcpy(&(*current_pcb).context, &ctx, sizeof(struct Context));
}

__attribute__((noreturn)) void scheduler_switch_to_next_process(void){
    /* Determine next process in a clear, safe way. Keep old index for tracing. */
    uint32_t old_idx = current_process_idx;

    /* Mark current as READY if valid */
    if (old_idx < PROCESS_COUNT_MAX && process_manager_state._process_used[old_idx]) {
        _process_list[old_idx].metadata.state = PROCESS_READY;
    }

    /* Find next ready process starting after the old index */
    uint32_t i = (old_idx + 1) % PROCESS_COUNT_MAX;
    uint32_t found = old_idx;
    for (uint32_t scanned = 0; scanned < PROCESS_COUNT_MAX; scanned++) {
        if (process_manager_state._process_used[i] && _process_list[i].metadata.state == PROCESS_READY) {
            found = i;
            break;
        }
        i++;
        if (i >= PROCESS_COUNT_MAX) i = 0;
    }

    next_process_idx = found;
    current_process_idx = found;

    /* change state */
    if (process_manager_state._process_used[current_process_idx]) {
        _process_list[current_process_idx].metadata.state = PROCESS_RUNNING;
    }

    /* Update TSS.esp0 so kernel has a valid stack when handling interrupts
     * that occur while the next process is running. */
    set_tss_kernel_current_stack();

    /* Switch address space if available */
    if (_process_list[current_process_idx].context.page_directory_virtual_addr) {
        paging_use_page_directory(_process_list[current_process_idx].context.page_directory_virtual_addr);
    }

    process_context_switch(_process_list[current_process_idx].context);
}

/* Non-fatal variant used for testing: update scheduler state but do not perform
 * the assembly-based context switch (no iret). This allows testing whether
 * the actual context-switch is the cause of IRQ/keyboard failure.
 */
void scheduler_switch_to_next_process_nonfatal(void) {
    uint32_t old_idx = current_process_idx;

    if (old_idx < PROCESS_COUNT_MAX && process_manager_state._process_used[old_idx]) {
        _process_list[old_idx].metadata.state = PROCESS_READY;
    }

    uint32_t i = (old_idx + 1) % PROCESS_COUNT_MAX;
    uint32_t found = old_idx;
    for (uint32_t scanned = 0; scanned < PROCESS_COUNT_MAX; scanned++) {
        if (process_manager_state._process_used[i] && _process_list[i].metadata.state == PROCESS_READY) {
            found = i;
            break;
        }
        i++;
        if (i >= PROCESS_COUNT_MAX) i = 0;
    }

    next_process_idx = found;
    current_process_idx = found;

    if (process_manager_state._process_used[current_process_idx]) {
        _process_list[current_process_idx].metadata.state = PROCESS_RUNNING;
    }

    set_tss_kernel_current_stack();

    if (_process_list[current_process_idx].context.page_directory_virtual_addr) {
        paging_use_page_directory(_process_list[current_process_idx].context.page_directory_virtual_addr);
    }

    /* Trace state update only */
    putchar('S', 0x0F);
    putchar(':', 0x0F);
    putchar('0' + (old_idx % 10), 0x0F);
    putchar('>', 0x0F);
    putchar('0' + (current_process_idx % 10), 0x0F);
    putchar('\n', 0x0F);

    /* Return to interrupted context (no iret performed here) */
}

void timer_isr(struct InterruptFrame frame){
    /* Throttle scheduling frequency to avoid instability. 
     * Only perform full context save & switch every SCHEDULER_TICK_INTERVAL ticks. 
     * On other ticks, just acknowledge the PIC and return quickly. */
    enum { SCHEDULER_TICK_INTERVAL = 2 };
    static uint32_t tick_count = 0;
    tick_count++;

    /* Quick path: fast ACK and return for most ticks */
    if ((tick_count % SCHEDULER_TICK_INTERVAL) != 0) {
        /* Heartbeat every 100 ticks to detect kernel liveness */
        if ((tick_count % 100) == 0) {
            putchar('T', 0x0F);
            putchar('\n', 0x0F);
        }
        pic_ack(IRQ_TIMER);
        return;
    }

    /* Slow path: perform context save and scheduling */
    struct ProcessControlBlock *cur = process_get_current_running_pcb_pointer();
    if (!cur) {
        pic_ack(IRQ_TIMER);
        return;
    }

    struct Context c = {0};
    c.cpu = frame.cpu;
    c.eip = frame.int_stack.eip;
    c.cs  = frame.int_stack.cs;
    c.eflags = frame.int_stack.eflags;
    /* Use inter-privilege stack values if present (when interrupt from user mode). 
     * Fallback to saved CPU stack value otherwise. */
    c.esp = (frame.int_stack.esp_privilege_change) ? frame.int_stack.esp_privilege_change : frame.cpu.stack.esp;
    c.ss  = (frame.int_stack.ss_privilege_change) ? frame.int_stack.ss_privilege_change : (0x20 | 0x3);
    c.page_directory_virtual_addr = paging_get_current_page_directory_addr();

    scheduler_save_context_to_current_running_pcb(c);

    pic_ack(IRQ_TIMER);

    /* Runtime-controlled behavior: when `scheduler_skip_context_switch` is
     * true, update scheduler state but do not perform the iret-based switch
     * (useful for diagnostics). Otherwise perform the normal scheduling
     * flow; however if there's only one active process we simply resume the
     * interrupted context instead of switching to self. */
    if (scheduler_skip_context_switch) {
        scheduler_switch_to_next_process_nonfatal();
        return;
    }

    if (process_manager_state.active_process_count <= 1) {
        return;
    }

    scheduler_switch_to_next_process();
}
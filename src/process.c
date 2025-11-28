#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"
#include "header/scheduler/scheduler.h"

struct ProcessManagerState process_manager_state = {
    .active_process_count = 0,
    ._process_used = {false}
};

struct ProcessControlBlock* process_get_current_running_pcb_pointer(void){
    for(uint32_t i=0; i<PROCESS_COUNT_MAX; i++){
        if(process_manager_state._process_used[i] &&
        _process_list[i].metadata.state == PROCESS_RUNNING){
            return &_process_list[i];
        }
    }
    return NULL;
}

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX] = {0};

int32_t process_create_user_process(struct EXT2DriverRequest request) {
    /* 0. Validasi & pengecekan beberapa kondisi kegagalan */
    int32_t retcode = PROCESS_CREATE_SUCCESS; 
    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX) { 
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    // Ensure entrypoint is not located at kernel's section at higher half
    if ((uint32_t) request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE) {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }

    // Check whether memory is enough for the executable and additional frame for user stack
    uint32_t page_frame_count_needed = ceil_div(request.buffer_size + PAGE_FRAME_SIZE, PAGE_FRAME_SIZE);
    if (!paging_allocate_check(page_frame_count_needed) || page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    // Process PCB 
    int32_t p_index = process_list_get_inactive_index();
    struct ProcessControlBlock *new_pcb = &(_process_list[p_index]);

    if (p_index < 0){
        // Tidak ada slot kosong
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    memcpy(&new_pcb->metadata.name, request.name, 8);
    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->metadata.state = PROCESS_READY;

    struct PageDirectory* current_pd = paging_get_current_page_directory_addr();
    /* 1. Pembuatan virtual address space baru dengan page directory */
    struct PageDirectory* new_pd = paging_create_new_page_directory();

    paging_allocate_user_page_frame(new_pd, request.buf);
    paging_allocate_user_page_frame(new_pd, (void *)0xBFFFFFFC);
    new_pcb->memory.virtual_addr_used[0] = request.buf;
    new_pcb->memory.virtual_addr_used[1] = (void *)0xBFFFFFFC;
    new_pcb->memory.page_frame_used_count = 2;

    process_manager_state._process_used[p_index] = true;
    process_manager_state.active_process_count++;

    /* Notify scheduler that process count changed: disable skip-mode when
     * more than one process exists so context-switching becomes active. */
    if (process_manager_state.active_process_count > 1) {
        scheduler_set_skip_context_switch(false);
    }

    paging_use_page_directory(new_pd);

    /* 2. Membaca dan melakukan load executable dari file system ke memory baru */
    uint32_t res_code = read(request);

    if(!!res_code){
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup;
    }

    paging_use_page_directory(current_pd);
    /* 3. Menyiapkan state & context awal untuk program */

    new_pcb->context.cpu.segment.ds = 0x20 | 0x3;
    new_pcb->context.cpu.segment.es = 0x20 | 0x3;
    new_pcb->context.cpu.segment.fs = 0x20 | 0x3;
    new_pcb->context.cpu.segment.gs = 0x20 | 0x3;
    new_pcb->context.eip = (uint32_t) request.buf;

    new_pcb->context.cpu.stack.ebp = 0xBFFFFFFC;
    new_pcb->context.cpu.stack.esp = 0xBFFFFFFC;

    new_pcb->context.page_directory_virtual_addr = new_pd;

    new_pcb->context.cs = 0x18 | 0x3;
    new_pcb->context.esp = 0xBFFFFFFC;
    new_pcb->context.ss = 0x20 | 0x3;
    
    new_pcb->context.eflags |= CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    /* 4. Mencatat semua informasi penting process ke metadata PCB */
    new_pcb->metadata.state = PROCESS_READY;

    
exit_cleanup:
    return retcode;
}

uint32_t process_list_get_inactive_index(){
    for(uint32_t i=0; i<PROCESS_COUNT_MAX; i++){
        if(!process_manager_state._process_used[i]){
            return i;
        }
    }
    return -1;
}

uint32_t ceil_div(uint32_t a, uint32_t b){
    uint32_t c = !!(a % b);
    return (a / b) + c;
}

uint32_t process_generate_new_pid(){
    uint32_t pid = process_list_get_inactive_index();
    return pid;
}
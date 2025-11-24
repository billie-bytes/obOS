#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

struct ProcessManagerState process_manager_state = {
    .active_process_count = 0,
    ._process_used = {false}
};

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

struct ProcessControlBlock* process_get_current_running_pcb_pointer(void){
    for(uint32_t i = 0; i < PROCESS_COUNT_MAX; i++){
        if(process_manager_state._process_used[i] && _process_list[i].metadata.state == PROCESS_RUNNING){
            return &_process_list[i];
        }
    }
    return NULL;
}

int32_t process_create_user_process(struct EXT2DriverRequest request) {
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

    new_pcb->metadata.pid = process_generate_new_pid();

exit_cleanup:
    return retcode;
}

bool process_destroy(uint32_t pid) {
    for(uint32_t i=0; i<PROCESS_COUNT_MAX; i++){
        if(_process_list[i].metadata.pid == pid){
            // destroy process
            process_manager_state.active_process_count--;
            process_manager_state._process_used[i] = false;

            // release page frame
            for(uint8_t j=0; j< PROCESS_PAGE_FRAME_COUNT_MAX; j++){
                // if(_process_list[i].memory.virtual_addr_used[j])
                paging_free_user_page_frame(_process_list[pid].context.page_directory_virtual_addr, _process_list[pid].memory.virtual_addr_used[j]);
            }

            // free page directory
            paging_free_page_directory(_process_list[pid].context.page_directory_virtual_addr);

            // clean the page_directory
            memset(&_process_list[i], 0x0, sizeof(struct ProcessControlBlock));

            return true;
        }
    }
    return false;
}
#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"
#include "header/scheduler/scheduler.h"
#include "header/text/framebuffer.h"

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX] = {0};

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

struct ProcessControlBlock* process_get_next_running_pcb_pointer(void) {
    for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.state == PROCESS_READY) {
            return &(_process_list[i]);
        }
    }
    return NULL;
}

int32_t process_list_get_inactive_index() {
    for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.state == PROCESS_TERMINATED) {
            return i;
        }
    }
    return -1;
}


uint32_t process_generate_new_pid() {
   for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.state == PROCESS_TERMINATED) {
            return i;
        }
    }
    return -1;
}

int32_t ceil_div(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

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

    if (p_index < 0){
        // Tidak ada slot kosong
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }
    
    struct ProcessControlBlock *new_pcb = &(_process_list[p_index]);

    /* 1. Pembuatan virtual address space baru dengan page directory */
    new_pcb->context.page_directory_virtual_addr = paging_create_new_page_directory();
    paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, (uint8_t *) 0);
    paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, (uint8_t *) 0xBFFFFFFC);

    // Copy request.name to local buffer before switching page directory
    char name_buffer[PROCESS_NAME_LENGTH_MAX];
    uint8_t copy_len = request.name_len < PROCESS_NAME_LENGTH_MAX - 1 ? request.name_len : PROCESS_NAME_LENGTH_MAX - 1;
    for (uint8_t i = 0; i < copy_len; i++) {
        name_buffer[i] = request.name[i];
    }
    name_buffer[copy_len] = '\0';
    request.name = name_buffer;
    request.name_len = copy_len;

    struct PageDirectory *old_page_directory = paging_get_current_page_directory_addr();
    paging_use_page_directory(new_pcb->context.page_directory_virtual_addr);

    /* 2. Membaca dan melakukan load executable dari file system ke memory baru */
    int8_t read_retcode = read(request);
    
    paging_use_page_directory(old_page_directory);
    
    // Check if read failed
    if (read_retcode != 0) {
        // Cleanup: free allocated page directory and frames
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup;
    }
    
    /* 3. Menyiapkan state & context awal untuk program */
    new_pcb->context.cpu.segment.ds = 0x20 | 0x3;
    new_pcb->context.cpu.segment.es = 0x20 | 0x3;
    new_pcb->context.cpu.segment.fs = 0x20 | 0x3;
    new_pcb->context.cpu.segment.gs = 0x20 | 0x3;
    
    new_pcb->context.cpu.stack.ebp = 0xBFFFFFFC;
    new_pcb->context.cpu.stack.esp = 0xBFFFFFFC;
    
    new_pcb->context.eip = 0x0;
    new_pcb->context.cs = 0x18 | 0x3;
    new_pcb->context.esp = 0xBFFFFFFC;
    new_pcb->context.ss = 0x20 | 0x3;
    
    new_pcb->context.eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    /* 4. Mencatat semua informasi penting process ke metadata PCB */
    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->metadata.state = PROCESS_READY;
    
    // Copy name from buffer (already copied before page directory switch)
    for (uint8_t i = 0; i < request.name_len; i++) {
        new_pcb->metadata.name[i] = request.name[i];
    }
    new_pcb->metadata.name[request.name_len] = '\0';
    new_pcb->metadata.name_len = request.name_len;
    
    // Mark process slot as used
    process_manager_state._process_used[p_index] = true;
    process_manager_state.active_process_count++;

    
exit_cleanup:
    return retcode;
}

bool process_destroy(uint32_t pid){
    for (int i = 0; i < PROCESS_COUNT_MAX; i++){
        if (_process_list[i].metadata.pid == pid && _process_list[i].metadata.state != PROCESS_TERMINATED){
            memset(&_process_list[i], 0, sizeof(struct ProcessControlBlock));
            _process_list[i].metadata.state = PROCESS_TERMINATED;
            process_manager_state.active_process_count--;
            return true;
        }
    }
    return false;
}

int32_t get_process_info(ProcessInfo *buffer, uint32_t bufsize){
    uint32_t count = 0;
    for (int i = 0; i < PROCESS_COUNT_MAX && count < bufsize; i++){
        // Only return processes that are actually used
        if (!process_manager_state._process_used[i]){
            continue;
        }
        
        buffer[count].pid = _process_list[i].metadata.pid;
        buffer[count].state = _process_list[i].metadata.state;
        
        // Copy name string to avoid kernel-user space pointer issue
        uint8_t len = _process_list[i].metadata.name_len;
        if (len > 31) len = 31;  // Leave room for null terminator
        
        // Copy name character by character
        for (uint8_t j = 0; j < len && _process_list[i].metadata.name[j] != '\0'; j++) {
            buffer[count].name[j] = _process_list[i].metadata.name[j];
        }
        buffer[count].name[len] = '\0';  // Null terminate
        buffer[count].name_len = len;
        
        count++;
    }
    return (int32_t)count;
}
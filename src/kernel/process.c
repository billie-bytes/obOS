#include "kernel/process.h"
#include "kernel/paging.h"
#include "lib/string.h"
#include "kernel/gdt.h"
#include "kernel/scheduler.h"
#include "kernel/framebuffer.h"

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

int32_t process_create_user_process(struct EXT2ProgramRequest request) {
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
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }
    
    struct ProcessControlBlock *new_pcb = &(_process_list[p_index]);

    /* Virtual address space baru dengan page directory */
    new_pcb->context.page_directory_virtual_addr = paging_create_new_page_directory();
    paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, (uint8_t *) 0);
    paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, (uint8_t *) 0xBFFFFFFC);
    
    
    // Buffer the arguments in kernel space before switching page directory
    // (This ensures we can still read request.argv from the old user process)
    char kernel_argv_buffer[16][128]; // Arbitrary max 16 arguments, 128 chars each
    uint32_t kernel_argc = request.argc > 16 ? 16 : request.argc;

    for (uint32_t i = 0; i < kernel_argc; i++) {
        uint32_t len = strlen(request.argv[i]);
        if (len > 127) len = 127;
        memcpy(kernel_argv_buffer[i], request.argv[i], len);
        kernel_argv_buffer[i][len] = '\0';
    }

    // Copy request.name to local buffer before switching page directory
    char name_buffer[PROCESS_NAME_LENGTH_MAX];
    uint8_t copy_len = request.name_len < PROCESS_NAME_LENGTH_MAX - 1 ? request.name_len : PROCESS_NAME_LENGTH_MAX - 1;
    for (uint8_t i = 0; i < copy_len; i++) {
        name_buffer[i] = request.name[i];
    }
    name_buffer[copy_len] = '\0';
    request.name = name_buffer;
    request.name_len = copy_len;

    // Switch to the new process's map
    struct PageDirectory *old_page_directory = paging_get_current_page_directory_addr();
    paging_use_page_directory(new_pcb->context.page_directory_virtual_addr);

    // only now it is safe to manipulate the stack at 0xBFFFFFFC
    uint32_t final_esp = 0xBFFFFFFC;
    { 
        uint32_t esp = 0xBFFFFFFC;
        uint32_t saved_addr[16];
        
        for(uint32_t i = 0; i < kernel_argc; ++i){
            size_t len = strlen(kernel_argv_buffer[i]) + 1;
            esp -= len;
            saved_addr[i] = esp;
            memcpy((void*)(esp), kernel_argv_buffer[i], len);
        }
        
        // Calculate exactly how many bytes we are about to push for the pointers/args
        // (null term) + (argc * 4) (pointers) + 4 (argv ptr) + 4 (argc) + 4 (ret addr)
        uint32_t push_bytes = 16 + (kernel_argc * 4);
        
        // (final_esp + 4) is a multiple of 16. This prevents GCC SSE instructions from crashing.
        while ((esp - push_bytes + 4) % 16 != 0) {
            esp--;
        }
        
        // argv[argc] null terminator
        esp -= 4;
        *((uint32_t*)esp) = 0;

        // argv pointers (pushed backwards so argv[0] is at lowest memory)
        for(uint32_t i = 0; i < kernel_argc; ++i){
            esp -= (sizeof(uint32_t));
            *((uint32_t*)esp) = saved_addr[kernel_argc - (i + 1)];
        }

        // pointer to where argv was stored in the stack 
        esp -= 4;
        *((uint32_t*)esp) = esp + 4;

        // argc parameter
        esp -= 4;
        *((uint32_t*)esp) = kernel_argc;

        // fake ret address
        esp -= 4;
        *((uint32_t*)esp) = 0;
        
        final_esp = esp; 

        /*
        STACK ILLUSTRATION
        EXAMPLE OF COMMAND BEING PASSED ONE ARGUMENT
        IN THIS CASE ITS "mkdir new_folder"

        HIGH
        > TOP OF STACK
        [esp + 30] "new_folder\0" <- argv[1]
        [esp + 24] "mkdir\0" <- argv[0]
        [esp + 20] "0x0" <- this is null terminator for the arguments
        [esp + 16] Pointer to [esp + 30], where argv[1] is stored
        [esp + 12] Pointer to [esp + 24], where argv[0] is stored
        [esp + 8] Pointer to [esp + 12], where the start of the pointer array is stored
        [esp + 4] 2 <- argument count (argc)
        [esp] 0x0 <- fake return address 
        LOW
        
        */
    }


    struct EXT2DriverRequest r = {
        .buf = request.buf,
        .name = request.name,
        .name_len = request.name_len,
        .parent_inode = request.parent_inode,
        .buffer_size = request.buffer_size,
        .is_folder = false
    };
    int8_t read_retcode = read(r);
    
    paging_use_page_directory(old_page_directory);
    
    // Check if read failed
    if (read_retcode != 0) {
        // Cleanup: free allocated page directory and frames
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup;
    }
    
    /* State & context awal untuk program */
    new_pcb->context.cpu.segment.ds = 0x20 | 0x3;
    new_pcb->context.cpu.segment.es = 0x20 | 0x3;
    new_pcb->context.cpu.segment.fs = 0x20 | 0x3;
    new_pcb->context.cpu.segment.gs = 0x20 | 0x3;
    
    // Assign the dynamically calculated stack pointer instead of the hardcoded top
    new_pcb->context.cpu.stack.ebp = final_esp;
    new_pcb->context.cpu.stack.esp = final_esp;
    new_pcb->context.esp = final_esp;
    
    new_pcb->context.eip = 0x0;
    new_pcb->context.cs = 0x18 | 0x3;
    new_pcb->context.ss = 0x20 | 0x3;
    
    new_pcb->context.eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->metadata.state = PROCESS_READY;
    new_pcb->metadata.cwd_inode = request.parent_inode;
    
    // Copy name from buffer (already copied before page directory switch)
    for (uint8_t i = 0; i < request.name_len; i++) {
        new_pcb->metadata.name[i] = request.name[i];
    }
    new_pcb->metadata.name[request.name_len] = '\0';
    new_pcb->metadata.name_len = request.name_len;
    new_pcb->metadata.flags = request.flags;
    
    // Mark process slot as used
    process_manager_state._process_used[p_index] = true;
    process_manager_state.active_process_count++;


    retcode = new_pcb->metadata.pid;
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
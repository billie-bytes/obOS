#ifndef _COMMAND_SYSCALL_H
#define _COMMAND_SYSCALL_H

#include <stdint.h>
#include "kernel/ext2.h"
struct cmos_reader {
    uint8_t century;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};
typedef enum PROCESS_FLAGS : uint8_t {
    FOREGROUND_PROCESS = 1, //00000001
    TAKES_INPUT = 2, //00000010
} FLAGS;

/**
 * @brief Core wrapper to trigger the OS system call interrupt (0x30).
 * * @param eax The syscall number to execute.
 * @param ebx The first argument passed to the syscall.
 * @param ecx The second argument passed to the syscall.
 * @param edx The third argument passed to the syscall.
 * @return Returns what is stored in eax
 */
static inline int32_t syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    int32_t retval;
    __asm__ volatile("int $0x30" : "=a" (retval) : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

// ==========================================
// Text Output Syscalls
// ==========================================

/**
 * @brief Prints a single character to the framebuffer.
 * * @param c The character to print.
 * @param color The VGA color attribute (e.g., COLOR_TXT).
 * @return None.
 */
static inline void sys_putchar(char c, uint8_t color) {
    syscall_do(5, (uint32_t)c, (uint32_t)color, 0);
}

static inline void sys_putchar_at(uint8_t row, uint8_t col, char c, uint8_t color) {
    __asm__ volatile("mov %0, %%edi" : : "r"((uint32_t)color));
    syscall_do(15, (uint32_t)row, (uint32_t)col, (uint32_t)(uint8_t)c);
}

/**
 * @brief Prints a string of characters to the framebuffer.
 * * @param s Pointer to the character array (string) to print.
 * @param len The number of characters to print.
 * @param color The VGA color attribute (e.g., COLOR_TXT).
 * @return None.
 */
static inline void sys_puts(const char *s, uint32_t len, uint8_t color) {
    syscall_do(6, (uint32_t)s, len, (uint32_t)color);
}

/**
 * @brief Fetches the next character from the keyboard buffer.
 * * @param out Pointer to a char where the pressed key will be stored. 
 * If no key is pressed, it typically writes 0 to this pointer.
 * @return None (output is written to the pointer).
 */
static inline void sys_getchar(char *out) {
    syscall_do(4, (uint32_t)out, 0, 0);
}


/**
 * @brief Initializes or activates the keyboard listener state for the current process.
 * * @param None.
 * @return None.
 */
static inline void sys_keyboard_activate(void) {
    syscall_do(7, 0, 0, 0);
}

// ==========================================
// Filesystem Syscalls
// ==========================================

/**
 * @brief Reads data directly from an inode.
 * @param inode The target Ext2 inode number.
 * @param buf Pointer to the destination buffer.
 * @param size Number of bytes to read.
 * @return int32_t: Bytes read, or an error code.
 */
static inline int32_t sys_read(uint32_t inode, void* buf, uint32_t size) {
    // eax=0, ebx=inode, ecx=buf, edx=size. Return value is caught natively by eax.
    return syscall_do(0, inode, (uint32_t)buf, size);
}

/**
 * @brief Reads directory table blocks from an inode.
 */
static inline int32_t sys_readdir(uint32_t inode, void* buf, uint32_t bufsize) {
    return syscall_do(1, inode, (uint32_t)buf, bufsize);
}

/**
 * @brief Writes data directly to an inode.
 */
static inline int32_t sys_write(uint32_t inode, void* buf, uint32_t size) {
    return syscall_do(2, inode, (uint32_t)buf, size);
}

/**
 * @brief Deletes a file or directory from the Ext2 filesystem.
 * * @param r Pointer to a configured EXT2DriverRequest struct.
 * @return int8_t: 0 on success, or a specific error code on failure.
 */
static inline int32_t sys_delete(const char* path) {
    return syscall_do(3, (uint32_t)path, 0, 0);
}

/**
 * @brief Creates a new directory at the specified path.
 */
static inline int32_t sys_mkdir(const char* path) {
    return syscall_do(25, (uint32_t)path, 0, 0);
}

// ==========================================
// System Syscalls
// ==========================================

/**
 * @brief Clears the screen (framebuffer) and resets the cursor.
 * * @param None.
 * @return None.
 */
static inline void sys_clear(void) {
    syscall_do(8, 0, 0, 0);
}

/**
 * @brief Terminates the currently running process and triggers the scheduler.
 * * @param None.
 * @return None (This function never returns to the caller).
 */
static inline void sys_exit(void) {
    syscall_do(10, 0, 0, 0);
}

/**
 * @brief Powers off or permanently halts the system via ACPI/port commands.
 * * @param None.
 * @return None (System halts).
 */
static inline void sys_shutdown(void) {
    syscall_do(21, 0, 0, 0);
}

static inline void sys_sleep(uint32_t ms) {
    syscall_do(9, ms, 0, 0);
}


static inline void sys_get_cmos(struct cmos_reader *buf) {
    syscall_do(14, (uint32_t)buf, 0, 0);
}

// ==========================================
// Process Management Syscalls
// ==========================================

/**
 * @brief Spawns a new user process from an executable file.
 * * @param req Pointer to an EXT2ProgramRequest containing process arguments and entry point.
 * @return int32_t: The PID of the new process on success, or an error code (< 0) on failure.
 */
static inline int32_t sys_exec(struct EXT2ProgramRequest* req) {
    int32_t retval;
    retval = syscall_do(11, (uint32_t)req, 0, 0);
    return retval;
}

static inline int32_t sys_fork() {
    int32_t new_pid = syscall_do(26, 0, 0, 0);
    return new_pid;
}

/**
 * @brief Forcibly terminates a process by its Process ID (PID).
 * * @param pid The ID of the process to terminate.
 * @return int32_t: 0 on success, or -1 if the process was not found.
 */
static inline int32_t sys_kill(uint32_t pid) {
    int32_t retval;
    retval = syscall_do(12, pid, 0, 0);
    return retval;
}

typedef enum {
    PROCESS_TERMINATED = 0,
    PROCESS_RUNNING = 1,
    PROCESS_READY = 2,
} PROCESS_STATE;

typedef struct {
    uint32_t pid;
    PROCESS_STATE state;
    char name[32];
    uint8_t name_len;
} ProcessInfo;

/**
 * @brief Retrieves a list of currently active processes.
 * * @param buffer Pointer to an array of ProcessInfo structs to populate.
 * @param bufsize The maximum number of processes the buffer can hold.
 * @return int32_t: The number of processes successfully written to the buffer.
 */
static inline int32_t sys_ps(ProcessInfo *buffer, uint32_t bufsize) {
    int32_t count;
    syscall_do(13, (uint32_t)buffer, bufsize, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(count));
    return count;
}

// ==========================================
// POSIX & Path Resolution Syscalls
// ==========================================

/**
 * @brief Retrieves the absolute string path of the current working directory.
 * @param buf Pointer to a user-allocated character array to store the path.
 * @param bufsize The maximum size of the buffer.
 * @return int32_t: The length of the path string on success, or -1 on failure (e.g., buffer too small).
 */
static inline int32_t sys_getcwd(char* buf, uint32_t bufsize) {
    int32_t retval;
    retval = syscall_do(22, (uint32_t)buf, bufsize, 0);
    return retval;
}

/**
 * @brief Changes the current working directory based on a given string path.
 * * @param path Pointer to a null-terminated string representing the target directory.
 * @return int32_t: 0 on success, or -1 on failure (e.g., folder not found or not a directory).
 */
static inline int32_t sys_chdir(const char* path) {
    int32_t retval;
    retval = syscall_do(23, (uint32_t)path, 0, 0);
    return retval;
}

/**
 * @brief (Legacy) Changes the current working directory based on a raw inode.
 * @note This maps to the same syscall (23) as sys_chdir but passes an integer instead of a pointer.
 * @param inode The target directory's Ext2 inode number.
 * @return None.
 */
static inline void sys_setcwd(uint32_t inode) {
    syscall_do(23, inode, 0, 0);
}

/**
 * @brief Retrieves the inode number and file type for a given path.
 * @param path Pointer to a null-terminated string representing the target file/folder.
 * @param out_type Pointer to a uint8_t where the file type (e.g., EXT2_FT_DIR) will be stored. Can be NULL.
 * @return uint32_t: The inode number if the target exists, or 0 if it does not exist.
 */
static inline uint32_t sys_stat(const char* path, uint8_t* out_type) {
    uint32_t target_inode;
    target_inode = syscall_do(24, (uint32_t)path, (uint32_t)out_type, 0); 
    return target_inode;
}

// Colors
#define COLOR_TXT 0x0F
#define COLOR_ERR 0x0C

#endif
#ifndef _COMMAND_SYSCALL_H
#define _COMMAND_SYSCALL_H

#include <stdint.h>
#include "../header/filesystem/ext2.h"

// Syscall wrapper
static inline void syscall_do(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("int $0x30" : : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

// Text output syscalls
static inline void sys_putchar(char c, uint8_t color) {
    syscall_do(5, (uint32_t)c, (uint32_t)color, 0);
}

static inline void sys_puts(const char *s, uint32_t len, uint8_t color) {
    syscall_do(6, (uint32_t)s, len, (uint32_t)color);
}

static inline void sys_getchar(char *out) {
    syscall_do(4, (uint32_t)out, 0, 0);
}

static inline void sys_keyboard_activate(void) {
    syscall_do(7, 0, 0, 0);
}

// Filesystem syscalls
static inline int8_t sys_read(struct EXT2DriverRequest* r) {
    int8_t rc;
    syscall_do(0, (uint32_t)r, (uint32_t)&rc, 0);
    return rc;
}

static inline int8_t sys_readdir(struct EXT2DriverRequest* r) {
    int8_t rc;
    syscall_do(1, (uint32_t)r, (uint32_t)&rc, 0);
    return rc;
}

static inline int8_t sys_write(struct EXT2DriverRequest* r) {
    int8_t rc;
    syscall_do(2, (uint32_t)r, (uint32_t)&rc, 0);
    return rc;
}

static inline int8_t sys_delete(struct EXT2DriverRequest* r) {
    int8_t rc;
    syscall_do(3, (uint32_t)r, (uint32_t)&rc, 0);
    return rc;
}

// System syscalls
static inline void sys_clear(void) {
    syscall_do(8, 0, 0, 0);
}

static inline void sys_exit(void) {
    syscall_do(10, 0, 0, 0);
}

// Process Management Syscalls
static inline int32_t sys_exec(struct EXT2DriverRequest* req) {
    int32_t retval;
    syscall_do(11, (uint32_t)req, 0, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

static inline int32_t sys_kill(uint32_t pid) {
    int32_t retval;
    syscall_do(12, pid, 0, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
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

static inline int32_t sys_ps(ProcessInfo *buffer, uint32_t bufsize) {
    int32_t count;
    syscall_do(13, (uint32_t)buffer, bufsize, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(count));
    return count;
}

static inline void sys_shutdown(void) {
    syscall_do(21, 0, 0, 0);
}

static inline uint32_t sys_getcwd(char* buf, uint32_t bufsize) {
    uint32_t retval;
    syscall_do(22, (uint32_t)buf, bufsize, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

static inline void sys_setcwd(uint32_t inode, const char* path) {
    syscall_do(23, inode, (uint32_t)path, 0);
}

// Syscall 24: Check existence of file and get the inode
static inline uint32_t sys_stat(uint32_t parent_inode, const char* name, uint8_t* out_type){
    uint32_t target_inode;
    syscall_do(24, parent_inode, (uint32_t)name, (uint32_t)out_type); // Fixed typo here
    __asm__ volatile("mov %%eax, %0": "=r"(target_inode));
    return target_inode;
}

// Colors
#define COLOR_TXT 0x0F
#define COLOR_ERR 0x0C

#endif

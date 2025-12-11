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

// Syscall 21: System shutdown
static inline void sys_shutdown(void) {
    syscall_do(21, 0, 0, 0);
}

// Syscall 22: Get current working directory
static inline uint32_t sys_getcwd(char* buf, uint32_t bufsize) {
    uint32_t retval;
    syscall_do(22, (uint32_t)buf, bufsize, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

// Syscall 23: Set current working directory
static inline void sys_setcwd(uint32_t inode, const char* path) {
    syscall_do(23, inode, (uint32_t)path, 0);
}

// Colors
#define COLOR_TXT 0x0F
#define COLOR_ERR 0x0C

#endif

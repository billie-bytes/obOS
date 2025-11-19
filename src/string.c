#include "header/stdlib/string.h" // Use quotes for your local header

/**
 * @brief Fills the first n bytes of the memory area pointed to by s
 * with the constant byte c.
 */
void* memset(void *s, int c, size_t n) {
    // Cast s to an unsigned char* to perform byte-level operations
    unsigned char *p = (unsigned char *)s;
    
    // Loop n times, setting each byte to c
    while (n--) {
        *p++ = (unsigned char)c;
    }
    
    // Return the original pointer
    return s;
}

/**
 * @brief Copies n bytes from memory area src to memory area dest.
 * The memory areas must not overlap. Use memmove for overlapping areas.
 * `restrict` keyword helps the compiler optimize, assuming no overlap.
 */
void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    // Cast pointers for byte-level operations
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    // Copy byte by byte
    while (n--) {
        *d++ = *s++;
    }
    
    // Return the original destination pointer
    return dest;
}

/**
 * @brief Compares the first n bytes of memory areas s1 and s2.
 */
int memcmp(const void *s1, const void *s2, size_t n) {
    // Cast pointers for byte-level operations
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    
    // Loop n times
    while (n--) {
        // If bytes differ, return the difference
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        
        // Move to the next byte
        p1++;
        p2++;
    }
    
    // If all n bytes are equal, return 0
    return 0;
}

/**
 * @brief Copies n bytes from memory area src to memory area dest.
 * This function correctly handles overlapping memory areas.
 */
void *memmove(void *dest, const void *src, size_t n) {
    // Cast pointers for byte-level operations
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    // If the destination buffer is "behind" the source buffer
    // (or they don't overlap), a simple forward copy is safe.
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } 
    // If the destination buffer is "ahead" of the source buffer,
    // a forward copy would overwrite data. We must copy backwards.
    else {
        // Point to the end of the buffers
        const unsigned char *lasts = s + (n - 1);
        unsigned char *lastd = d + (n - 1);
        
        // Copy byte by byte from end to beginning
        while (n--) {
            *lastd-- = *lasts--;
        }
    }
    
    // Return the original destination pointer
    return dest;
}

/**
 * @brief Compares two null-terminated strings, s1 and s2.
 */
int strcmp(const char *s1, const char *s2) {
    // Loop while both characters are equal and neither is a null terminator
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    // Return the difference of the characters that broke the loop.
    // This will be 0 if both are null terminators (end of string).
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        // Cast to unsigned char for proper comparison of extended ASCII
        unsigned char u1 = (unsigned char)*s1;
        unsigned char u2 = (unsigned char)*s2;

        if (u1 != u2) {
            return u1 - u2;
        }

        // If we hit the null terminator, strings are equal up to here
        if (u1 == '\0') {
            return 0;
        }

        s1++;
        s2++;
        n--;
    }
    // If n reaches 0, the strings matched for those n characters
    return 0;
}

/**
 * @brief Calculate the length of a string.
 * * @param str The string to calculate the length of.
 * @return The length of the string.
 */
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}
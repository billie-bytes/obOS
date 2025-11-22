#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>
#include <stddef.h>

/**
 * C standard memset, check man memset or
 * https://man7.org/linux/man-pages/man3/memset.3.html for more details
 * * @param s Pointer to memory area to set
 * @param c Constant byte value for filling memory area
 * @param n Memory area size in byte 
 * * @return Pointer s
*/
void* memset(void *s, int c, size_t n);

/**
 * C standard memcpy, check man memcpy or
 * https://man7.org/linux/man-pages/man3/memcpy.3.html for more details
 * * @param dest Starting location for memory area to set
 * @param src Pointer to source memory
 * @param n Memory area size in byte 
 * * @return Pointer dest
*/
void* memcpy(void* restrict dest, const void* restrict src, size_t n);

/**
 * C standard memcmp, check man memcmp or
 * https://man7.org/linux/man-pages/man3/memcmp.3.html for more details
 * * @param s1 Pointer to first memory area
 * @param s2 Pointer to second memory area
 * @param n Memory area size in byte 
 * * @return Integer as error code, zero for equality, non-zero for inequality
*/
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * C standard memmove, check man memmove or
 * https://man7.org/linux/man-pages/man3/memmove.3.html for more details
 * * @param dest Pointer to destination memory
 * @param src Pointer to source memory
 * @param n Memory area size in byte 
 * * @return Pointer dest
*/
void *memmove(void *dest, const void *src, size_t n);

/**
 * C standard strcmp, check man strcmp or
 * https://man7.org/linux/man-pages/man3/strcmp.3.html for more details
 *
 * Compares two null-terminated strings.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2.
 */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *str);

/**
 * C standard strlen, calculates the length of a null-terminated string
 * 
 * @param str Pointer to null-terminated string
 * @return Length of the string (excluding null terminator)
 */
size_t strlen(const char *str);

char*  strcpy(char *dest, const char *src);
char*  strncpy(char *dest, const char *src, size_t n);
char*  strcat(char *dest, const char *src);
char*  strncat(char *dest, const char *src, size_t n);
char*  strchr(const char *s, int c);
char*  strrchr(const char *s, int c);
char*  strtok(char *s, const char *delim);

#endif
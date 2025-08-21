/*****
 *
 * Description: Security and Hardening Headers
 * 
 * Copyright (c) 2025, Ron Dilley
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ****/

#ifndef SECURITY_DOT_H
#define SECURITY_DOT_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "../include/sysdep.h"
#include "../include/common.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

/* Security macros and constants */
#define SECURE_PATH_MAX 4096
#define SECURE_LINE_MAX 8192
#define SECURE_BUFFER_MAX 65536

/* Secure string operations */
#define SECURE_STRNCPY(dst, src, size) do { \
    strncpy(dst, src, size - 1); \
    dst[size - 1] = '\0'; \
} while(0)

#define SECURE_STRNCAT(dst, src, size) do { \
    size_t dst_len = strlen(dst); \
    if (dst_len < size - 1) { \
        strncat(dst, src, size - dst_len - 1); \
    } \
} while(0)

/* Secure memory operations */
#define SECURE_MEMSET(ptr, val, size) do { \
    volatile unsigned char *p = (volatile unsigned char *)ptr; \
    while (size--) *p++ = val; \
} while(0)

#define SECURE_FREE(ptr) do { \
    if (ptr) { \
        free(ptr); \
        ptr = NULL; \
    } \
} while(0)

/* Bounds checking macros */
#define CHECK_BOUNDS(value, min, max) do { \
    if ((value) < (min) || (value) > (max)) { \
        fprintf(stderr, "SECURITY: Bounds check failed at %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

#define CHECK_NULL(ptr) do { \
    if (!(ptr)) { \
        fprintf(stderr, "SECURITY: Null pointer at %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

#define CHECK_BUFFER_SIZE(size) do { \
    if ((size) > SECURE_BUFFER_MAX) { \
        fprintf(stderr, "SECURITY: Buffer size too large at %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

/* Integer overflow protection */
#define CHECK_ADD_OVERFLOW(a, b, result) do { \
    if ((a) > 0 && (b) > 0 && (a) > (SIZE_MAX - (b))) { \
        fprintf(stderr, "SECURITY: Addition overflow at %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    } \
    result = (a) + (b); \
} while(0)

#define CHECK_MUL_OVERFLOW(a, b, result) do { \
    if ((a) > 0 && (b) > 0 && (a) > (SIZE_MAX / (b))) { \
        fprintf(stderr, "SECURITY: Multiplication overflow at %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    } \
    result = (a) * (b); \
} while(0)

/* Function prototypes */
int secure_open(const char *pathname, int flags, mode_t mode);
FILE *secure_fopen(const char *pathname, const char *mode);
int secure_access(const char *pathname, int mode);
void secure_cleanup_temp_files(void);
int secure_validate_path(const char *path);
int secure_validate_filename(const char *filename);
size_t secure_strlcpy(char *dst, const char *src, size_t size);
size_t secure_strlcat(char *dst, const char *src, size_t size);
void *secure_malloc(size_t size);
void *secure_calloc(size_t nmemb, size_t size);
void *secure_realloc(void *ptr, size_t size);
void secure_free_and_null(void **ptr);
int secure_random_bytes(unsigned char *buf, size_t len);
void secure_clear_memory(void *ptr, size_t len);


/* Privilege dropping functions */
int drop_privileges(void);
int restore_privileges(void);

#endif /* SECURITY_DOT_H */
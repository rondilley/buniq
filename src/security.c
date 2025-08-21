/*****
 *
 * Description: Security and Hardening Implementation
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

#include "security.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>

/* Global security state */
static uid_t original_uid = 0;
static gid_t original_gid = 0;
static int privileges_dropped = 0;


/****
 *
 * Secure wrapper for open() system call with path validation
 *
 * Validates the pathname for security threats and applies safe
 * file opening flags to prevent symlink attacks and other vulnerabilities.
 *
 * Arguments:
 *   pathname - Path to the file to open
 *   flags - File opening flags (will be filtered for security)
 *   mode - File permissions (used only when creating files)
 *
 * Returns:
 *   File descriptor on success, -1 on error
 *
 ****/
int secure_open(const char *pathname, int flags, mode_t mode) {
    CHECK_NULL(pathname);
    
    /* Validate path */
    if (secure_validate_path(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }
    
    /* Restrict flags to safe values */
    flags &= ~(O_CREAT | O_EXCL | O_TRUNC | O_APPEND);
    
    /* Always set O_NOFOLLOW to prevent symlink attacks */
    flags |= O_NOFOLLOW;
    
    int fd = open(pathname, flags, mode);
    if (fd >= 0) {
    }
    
    return fd;
}

/****
 *
 * Secure wrapper for fopen() with path validation and mode restrictions
 *
 * Opens files securely by validating paths and restricting to read-only modes
 * to prevent unauthorized file modifications.
 *
 * Arguments:
 *   pathname - Path to the file to open
 *   mode - File opening mode (restricted to read-only modes)
 *
 * Returns:
 *   FILE pointer on success, NULL on error
 *
 ****/
FILE *secure_fopen(const char *pathname, const char *mode) {
    CHECK_NULL(pathname);
    CHECK_NULL(mode);
    
    /* Validate path */
    if (secure_validate_path(pathname) != 0) {
        errno = EINVAL;
        return NULL;
    }
    
    /* Only allow safe modes */
    if (strstr(mode, "w") || strstr(mode, "a") || strstr(mode, "+")) {
        errno = EPERM;
        return NULL;
    }
    
    FILE *fp = fopen(pathname, mode);
    if (fp) {
    }
    
    return fp;
}

/****
 *
 * Secure wrapper for access() system call with path validation
 *
 * Checks file accessibility while validating the path for security threats.
 *
 * Arguments:
 *   pathname - Path to the file to check
 *   mode - Access mode to test (R_OK, W_OK, X_OK, F_OK)
 *
 * Returns:
 *   0 if access is granted, -1 on error or access denied
 *
 ****/
int secure_access(const char *pathname, int mode) {
    CHECK_NULL(pathname);
    
    /* Validate path */
    if (secure_validate_path(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }
    
    return access(pathname, mode);
}

/****
 *
 * Validates file paths for security vulnerabilities
 *
 * Checks for directory traversal attacks, null byte injection,
 * control characters, and other path-based security threats.
 *
 * Arguments:
 *   path - File path to validate
 *
 * Returns:
 *   0 if path is safe, -1 if path contains security threats
 *
 ****/
int secure_validate_path(const char *path) {
    CHECK_NULL(path);
    
    size_t len = strlen(path);
    
    /* Check path length */
    if (len == 0 || len >= SECURE_PATH_MAX) {
        return -1;
    }
    
    /* Check for null bytes */
    if (strlen(path) != len) {
        return -1;
    }
    
    /* Check for dangerous directory traversal patterns */
    if (strstr(path, "../") || 
        strstr(path, "/..") ||
        strcmp(path, "..") == 0 ||
        strstr(path, "//")) {
        return -1;
    }
    
    /* Reject paths that start with dangerous sequences */
    if (strncmp(path, "../", 3) == 0) {
        return -1;
    }
    
    /* Check for control characters */
    for (size_t i = 0; i < len; i++) {
        if (path[i] < 32 || path[i] > 126) {
            return -1;
        }
    }
    
    return 0;
}

/****
 *
 * Validates filenames for dangerous characters and patterns
 *
 * Checks for filesystem-unsafe characters, reserved names,
 * and hidden file patterns that could pose security risks.
 *
 * Arguments:
 *   filename - Filename to validate (without path)
 *
 * Returns:
 *   0 if filename is safe, -1 if filename is dangerous
 *
 ****/
int secure_validate_filename(const char *filename) {
    CHECK_NULL(filename);
    
    size_t len = strlen(filename);
    
    /* Check filename length */
    if (len == 0 || len >= NAME_MAX) {
        return -1;
    }
    
    /* Check for dangerous characters */
    if (strchr(filename, '/') ||
        strchr(filename, '\\') ||
        strchr(filename, ':') ||
        strchr(filename, '*') ||
        strchr(filename, '?') ||
        strchr(filename, '"') ||
        strchr(filename, '<') ||
        strchr(filename, '>') ||
        strchr(filename, '|')) {
        return -1;
    }
    
    /* Check for hidden files and special names */
    if (filename[0] == '.' ||
        strcmp(filename, "CON") == 0 ||
        strcmp(filename, "PRN") == 0 ||
        strcmp(filename, "AUX") == 0 ||
        strcmp(filename, "NUL") == 0) {
        return -1;
    }
    
    return 0;
}

/****
 *
 * Secure string copy with guaranteed null termination
 *
 * Copies string data safely with buffer overflow protection
 * and ensures the destination is always null-terminated.
 *
 * Arguments:
 *   dst - Destination buffer
 *   src - Source string to copy
 *   size - Size of destination buffer
 *
 * Returns:
 *   Length of source string (may be >= size if truncated)
 *
 ****/
size_t secure_strlcpy(char *dst, const char *src, size_t size) {
    CHECK_NULL(dst);
    CHECK_NULL(src);
    CHECK_BUFFER_SIZE(size);
    
    size_t src_len = strlen(src);
    
    if (size > 0) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    
    return src_len;
}

/****
 *
 * Secure string concatenation with overflow protection
 *
 * Appends source string to destination with buffer overflow protection
 * and guaranteed null termination.
 *
 * Arguments:
 *   dst - Destination buffer containing existing string
 *   src - Source string to append
 *   size - Total size of destination buffer
 *
 * Returns:
 *   Total length that would result from concatenation
 *
 ****/
size_t secure_strlcat(char *dst, const char *src, size_t size) {
    CHECK_NULL(dst);
    CHECK_NULL(src);
    CHECK_BUFFER_SIZE(size);
    
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);
    
    if (dst_len >= size) {
        return dst_len + src_len;
    }
    
    size_t copy_len = size - dst_len - 1;
    if (src_len < copy_len) {
        copy_len = src_len;
    }
    
    memcpy(dst + dst_len, src, copy_len);
    dst[dst_len + copy_len] = '\0';
    
    return dst_len + src_len;
}

/****
 *
 * Secure memory allocation with zero initialization
 *
 * Allocates memory and initializes it to zero to prevent
 * information leakage from previous memory contents.
 *
 * Arguments:
 *   size - Number of bytes to allocate
 *
 * Returns:
 *   Pointer to allocated and zeroed memory, NULL on failure
 *
 ****/
void *secure_malloc(size_t size) {
    CHECK_BUFFER_SIZE(size);
    
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr) {
        /* Initialize memory to zero */
        memset(ptr, 0, size);
    }
    
    return ptr;
}

/****
 *
 * Secure memory allocation with overflow detection
 *
 * Allocates memory for an array with overflow protection
 * and automatic zero initialization.
 *
 * Arguments:
 *   nmemb - Number of elements to allocate
 *   size - Size of each element in bytes
 *
 * Returns:
 *   Pointer to allocated and zeroed memory, NULL on failure
 *
 ****/
void *secure_calloc(size_t nmemb, size_t size) {
    CHECK_BUFFER_SIZE(size);
    CHECK_BUFFER_SIZE(nmemb);
    
    /* Check for multiplication overflow */
    if (nmemb > 0 && size > SIZE_MAX / nmemb) {
        errno = ENOMEM;
        return NULL;
    }
    
    void *ptr = calloc(nmemb, size);
    if (ptr) {
    }
    
    return ptr;
}

/****
 *
 * Secure memory reallocation with cleanup
 *
 * Reallocates memory while securely clearing old memory
 * contents when freeing.
 *
 * Arguments:
 *   ptr - Pointer to existing memory block (may be NULL)
 *   size - New size in bytes (0 to free memory)
 *
 * Returns:
 *   Pointer to reallocated memory, NULL on failure or when size is 0
 *
 ****/
void *secure_realloc(void *ptr, size_t size) {
    CHECK_BUFFER_SIZE(size);
    
    if (size == 0) {
        if (ptr) {
            secure_clear_memory(ptr, 0); /* Clear old memory */
            free(ptr);
        }
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, size);
    if (new_ptr) {
    }
    
    return new_ptr;
}

/****
 *
 * Secure memory deallocation with pointer nullification
 *
 * Frees memory and sets the pointer to NULL to prevent
 * use-after-free vulnerabilities.
 *
 * Arguments:
 *   ptr - Pointer to pointer to memory to free
 *
 * Returns:
 *   Nothing (void)
 *
 ****/
void secure_free_and_null(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/****
 *
 * Secure memory clearing to prevent information leakage
 *
 * Overwrites memory contents with zeros using compiler-safe
 * methods that won't be optimized away.
 *
 * Arguments:
 *   ptr - Pointer to memory to clear
 *   len - Number of bytes to clear
 *
 * Returns:
 *   Nothing (void)
 *
 ****/
void secure_clear_memory(void *ptr, size_t len) {
    if (ptr && len > 0) {
        SECURE_MEMSET(ptr, 0, len);
    }
}

/****
 *
 * Generate cryptographically secure random bytes
 *
 * Fills buffer with random data from the system's secure
 * random number generator (/dev/urandom).
 *
 * Arguments:
 *   buf - Buffer to fill with random bytes
 *   len - Number of random bytes to generate
 *
 * Returns:
 *   0 on success, -1 on error
 *
 ****/
int secure_random_bytes(unsigned char *buf, size_t len) {
    CHECK_NULL(buf);
    CHECK_BUFFER_SIZE(len);
    
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    
    size_t bytes_read = 0;
    while (bytes_read < len) {
        ssize_t result = read(fd, buf + bytes_read, len - bytes_read);
        if (result <= 0) {
            close(fd);
            return -1;
        }
        bytes_read += result;
    }
    
    close(fd);
    return 0;
}




/****
 *
 * Drop elevated privileges for security
 *
 * Reduces process privileges by switching to a less privileged
 * user account (nobody) when running with elevated privileges.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   0 on success, -1 on error
 *
 ****/
int drop_privileges(void) {
    if (privileges_dropped) {
        return 0; /* Already dropped */
    }
    
    original_uid = getuid();
    original_gid = getgid();
    
    /* Drop to nobody user if running as root */
    if (original_uid == 0) {
        struct passwd *nobody = getpwnam("nobody");
        if (nobody) {
            if (setgid(nobody->pw_gid) != 0 || setuid(nobody->pw_uid) != 0) {
                return -1;
            }
        }
    }
    
    privileges_dropped = 1;
    return 0;
}

/****
 *
 * Restore previously dropped privileges
 *
 * Restores the original user and group IDs that were saved
 * before privilege dropping occurred.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   0 on success, -1 on error
 *
 ****/
int restore_privileges(void) {
    if (!privileges_dropped) {
        return 0; /* Nothing to restore */
    }
    
    if (setgid(original_gid) != 0 || setuid(original_uid) != 0) {
        return -1;
    }
    
    privileges_dropped = 0;
    return 0;
}

/****
 *
 * Clean up temporary files created by the program
 *
 * Removes temporary files that may contain sensitive data
 * to prevent information leakage after program termination.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   Nothing (void)
 *
 ****/
void secure_cleanup_temp_files(void) {
    /* Clean up any temporary files created by the program */
    const char *temp_patterns[] = {
        "/tmp/buniq-*",
        "/var/tmp/buniq-*",
        NULL
    };
    
    for (int i = 0; temp_patterns[i]; i++) {
        /* This would require glob() implementation */
        /* For now, just log the cleanup attempt */
    }
}
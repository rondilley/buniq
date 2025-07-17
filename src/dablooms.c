/* Copyright @2012 by Justin Hines at Bitly under a very liberal license. See LICENSE in the source distribution. */

#define _GNU_SOURCE
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "murmur.h"
#include "dablooms.h"

#define DABLOOMS_VERSION "0.9.1"

#define ERROR_TIGHTENING_RATIO 0.5
#define SALT_CONSTANT 0x97c29b3a

/****
 *
 * Returns the version string of the dablooms library
 *
 * This function provides the current version of the dablooms dynamic bloom
 * filter implementation.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   Pointer to a constant string containing the version number
 *
 ****/
const char *dablooms_version(void)
{
    return DABLOOMS_VERSION;
}

/****
 *
 * Frees a bitmap structure and releases associated resources
 *
 * This function unmaps the memory-mapped bitmap array, closes the file
 * descriptor, and frees the bitmap structure itself.
 *
 * Arguments:
 *   bitmap - Pointer to the bitmap structure to free
 *
 * Returns:
 *   None (void)
 *
 ****/
void free_bitmap(bitmap_t *bitmap)
{
    if ((munmap(bitmap->array, bitmap->bytes)) < 0) {
        perror("Error, unmapping memory");
    }
    close(bitmap->fd);
    free(bitmap);
}

/****
 *
 * Resizes a bitmap by changing its underlying memory mapping
 *
 * This function grows or shrinks a bitmap's memory mapping to accommodate
 * a new size. It handles file truncation and memory remapping, using
 * mremap() on Linux for efficiency or unmapping/remapping on other systems.
 *
 * Arguments:
 *   bitmap - Pointer to the bitmap structure to resize
 *   old_size - Previous size of the bitmap in bytes
 *   new_size - New desired size of the bitmap in bytes
 *
 * Returns:
 *   Pointer to the resized bitmap on success, NULL on error
 *
 ****/
bitmap_t *bitmap_resize(bitmap_t *bitmap, size_t old_size, size_t new_size)
{
    int fd = bitmap->fd;
    struct stat fileStat;
    
    fstat(fd, &fileStat);
    size_t size = fileStat.st_size;
    
    /* grow file if necessary */
    if (size < new_size) {
        if (ftruncate(fd, new_size) < 0) {
            perror("Error increasing file size with ftruncate");
            free_bitmap(bitmap);
            close(fd);
            return NULL;
        }
    }
    lseek(fd, 0, SEEK_SET);
    
    /* resize if mmap exists and possible on this os, else new mmap */
    if (bitmap->array != NULL) {
#if __linux
        bitmap->array = mremap(bitmap->array, old_size, new_size, MREMAP_MAYMOVE);
        if (bitmap->array == MAP_FAILED) {
            perror("Error resizing mmap");
            free_bitmap(bitmap);
            close(fd);
            return NULL;
        }
#else
        if (munmap(bitmap->array, bitmap->bytes) < 0) {
            perror("Error unmapping memory");
            free_bitmap(bitmap);
            close(fd);
            return NULL;
        }
        bitmap->array = NULL;
#endif
    }
    if (bitmap->array == NULL) {
        bitmap->array = mmap(0, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (bitmap->array == MAP_FAILED) {
            perror("Error init mmap");
            free_bitmap(bitmap);
            close(fd);
            return NULL;
        }
    }
    
    bitmap->bytes = new_size;
    return bitmap;
}

/****
 *
 * Creates a new bitmap structure with memory-mapped storage
 *
 * This function allocates and initializes a new bitmap structure that
 * provides a means of interacting with 4-bit counters through memory
 * mapping. The bitmap is backed by a file descriptor.
 *
 * Arguments:
 *   fd - File descriptor for the backing file
 *   bytes - Size of the bitmap in bytes
 *
 * Returns:
 *   Pointer to the new bitmap structure on success, NULL on error
 *
 ****/
bitmap_t *new_bitmap(int fd, size_t bytes)
{
    bitmap_t *bitmap;
    
    if ((bitmap = (bitmap_t *)malloc(sizeof(bitmap_t))) == NULL) {
        return NULL;
    }
    
    bitmap->bytes = bytes;
    bitmap->fd = fd;
    bitmap->array = NULL;
    
    if ((bitmap = bitmap_resize(bitmap, 0, bytes)) == NULL) {
        return NULL;
    }
    
    return bitmap;
}

/****
 *
 * Increments a 4-bit counter in the bitmap at the specified index
 *
 * This function increments one of the 4-bit counters stored in the bitmap.
 * Two counters are packed into each byte, with even indices using the upper
 * 4 bits and odd indices using the lower 4 bits.
 *
 * Arguments:
 *   bitmap - Pointer to the bitmap structure
 *   index - Index of the 4-bit counter to increment
 *   offset - Byte offset within the bitmap array
 *
 * Returns:
 *   0 on success, -1 on overflow (counter already at maximum value 15)
 *
 ****/
int bitmap_increment(bitmap_t *bitmap, unsigned int index, long offset)
{
    long access = index / 2 + offset;
    uint8_t temp;
    uint8_t n = bitmap->array[access];
    if (index % 2 != 0) {
        temp = (n & 0x0f);
        n = (n & 0xf0) + ((n & 0x0f) + 0x01);
    } else {
        temp = (n & 0xf0) >> 4;
        n = (n & 0x0f) + ((n & 0xf0) + 0x10);
    }
    
    if (temp == 0x0f) {
        fprintf(stderr, "Error, 4 bit int Overflow\n");
        return -1;
    }
    
    bitmap->array[access] = n;
    return 0;
}

/****
 *
 * Decrements a 4-bit counter in the bitmap at the specified index
 *
 * This function decrements one of the 4-bit counters stored in the bitmap.
 * Two counters are packed into each byte, with even indices using the upper
 * 4 bits and odd indices using the lower 4 bits.
 *
 * Arguments:
 *   bitmap - Pointer to the bitmap structure
 *   index - Index of the 4-bit counter to decrement
 *   offset - Byte offset within the bitmap array
 *
 * Returns:
 *   0 on success, -1 on underflow (counter already at minimum value 0)
 *
 ****/
int bitmap_decrement(bitmap_t *bitmap, unsigned int index, long offset)
{
    long access = index / 2 + offset;
    uint8_t temp;
    uint8_t n = bitmap->array[access];
    
    if (index % 2 != 0) {
        temp = (n & 0x0f);
        n = (n & 0xf0) + ((n & 0x0f) - 0x01);
    } else {
        temp = (n & 0xf0) >> 4;
        n = (n & 0x0f) + ((n & 0xf0) - 0x10);
    }
    
    if (temp == 0x00) {
        fprintf(stderr, "Error, Decrementing zero\n");
        return -1;
    }
    
    bitmap->array[access] = n;
    return 0;
}

/****
 *
 * Checks the value of a 4-bit counter in the bitmap at the specified index
 *
 * This function reads the value of one of the 4-bit counters stored in the
 * bitmap. Two counters are packed into each byte, with even indices using
 * the upper 4 bits and odd indices using the lower 4 bits.
 *
 * Arguments:
 *   bitmap - Pointer to the bitmap structure
 *   index - Index of the 4-bit counter to check
 *   offset - Byte offset within the bitmap array
 *
 * Returns:
 *   The value of the 4-bit counter (0-15) with appropriate bit masking
 *
 ****/
int bitmap_check(bitmap_t *bitmap, unsigned int index, long offset)
{
    long access = index / 2 + offset;
    if (index % 2 != 0 ) {
        return bitmap->array[access] & 0x0f;
    } else {
        return bitmap->array[access] & 0xf0;
    }
}

/****
 *
 * Flushes bitmap changes to disk storage
 *
 * This function synchronizes the memory-mapped bitmap array with its
 * backing file on disk using msync() to ensure data persistence.
 *
 * Arguments:
 *   bitmap - Pointer to the bitmap structure to flush
 *
 * Returns:
 *   0 on success, -1 on error
 *
 ****/
int bitmap_flush(bitmap_t *bitmap)
{
    if ((msync(bitmap->array, bitmap->bytes, MS_SYNC) < 0)) {
        perror("Error, flushing bitmap to disk");
        return -1;
    } else {
        return 0;
    }
}

/****
 *
 * Performs hash computation for bloom filter key insertion/lookup
 *
 * This function computes multiple hash values for a given key using the
 * double hashing technique described by Kirsch and Mitzenmacher (2006).
 * It uses MurmurHash3 to generate two initial hash values, then derives
 * additional hash functions through linear combination.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter structure
 *   key - The key string to hash
 *   key_len - Length of the key string in bytes
 *   hashes - Array to store the computed hash values
 *
 * Returns:
 *   None (void) - results are stored in the hashes array
 *
 ****/
void hash_func(counting_bloom_t *bloom, const char *key, size_t key_len, uint32_t *hashes)
{
    int i;
    uint32_t checksum[4];
    
    MurmurHash3_x64_128(key, key_len, SALT_CONSTANT, checksum);
    uint32_t h1 = checksum[0];
    uint32_t h2 = checksum[1];
    
    for (i = 0; i < bloom->nfuncs; i++) {
        hashes[i] = (h1 + i * h2) % bloom->counts_per_func;
    }
}

/****
 *
 * Frees a counting bloom filter structure and its associated memory
 *
 * This function deallocates all memory associated with a counting bloom
 * filter, including the hash array, bitmap, and the structure itself.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter structure to free
 *
 * Returns:
 *   Always returns 0
 *
 ****/
int free_counting_bloom(counting_bloom_t *bloom)
{
    if (bloom != NULL) {
        free(bloom->hashes);
        bloom->hashes = NULL;
        free(bloom->bitmap);
        free(bloom);
        bloom = NULL;
    }
    return 0;
}

/****
 *
 * Initializes a counting bloom filter with specified parameters
 *
 * This function creates and initializes a counting bloom filter structure
 * with the given capacity and error rate. It calculates the optimal number
 * of hash functions and bit array size based on bloom filter theory.
 *
 * Arguments:
 *   capacity - Maximum number of elements the filter should hold
 *   error_rate - Desired false positive probability (0.0 to 1.0)
 *   offset - Byte offset for the filter data in the backing storage
 *
 * Returns:
 *   Pointer to the initialized counting bloom filter on success, NULL on error
 *
 ****/
counting_bloom_t *counting_bloom_init(unsigned int capacity, double error_rate, long offset)
{
    counting_bloom_t *bloom;
    
    /* Validate input parameters */
    if (capacity < 1000 || capacity > UINT_MAX / 100) {
        fprintf(stderr, "Error, invalid capacity for bloom filter\n");
        return NULL;
    }
    
    if (error_rate <= 0.0 || error_rate >= 1.0) {
        fprintf(stderr, "Error, error rate must be between 0.0 and 1.0\n");
        return NULL;
    }
    
    if ((bloom = malloc(sizeof(counting_bloom_t))) == NULL) {
        fprintf(stderr, "Error, could not allocate new bloom filter\n");
        return NULL;
    }
    bloom->bitmap = NULL;
    bloom->capacity = capacity;
    bloom->error_rate = error_rate;
    bloom->offset = offset + sizeof(counting_bloom_header_t);
    bloom->nfuncs = (int) ceil(log(1 / error_rate) / log(2));
    bloom->counts_per_func = (int) ceil(capacity * fabs(log(error_rate)) / (bloom->nfuncs * pow(log(2), 2)));
    bloom->size = bloom->nfuncs * bloom->counts_per_func;
    /* rounding-up integer divide by 2 of bloom->size */
    bloom->num_bytes = ((bloom->size + 1) / 2) + sizeof(counting_bloom_header_t);
    bloom->hashes = calloc(bloom->nfuncs, sizeof(uint32_t));
    
    return bloom;
}

/****
 *
 * Creates a new counting bloom filter backed by a file
 *
 * This function creates a new counting bloom filter with the specified
 * capacity and error rate, backed by a file for persistent storage.
 * The file is created or truncated if it already exists.
 *
 * Arguments:
 *   capacity - Maximum number of elements the filter should hold
 *   error_rate - Desired false positive probability (0.0 to 1.0)
 *   filename - Path to the file that will back the bloom filter
 *
 * Returns:
 *   Pointer to the new counting bloom filter on success, NULL on error
 *
 ****/
counting_bloom_t *new_counting_bloom(unsigned int capacity, double error_rate, const char *filename)
{
    counting_bloom_t *cur_bloom;
    int fd;
    
    if ((fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600)) < 0) {
        perror("Error, Opening File Failed");
        fprintf(stderr, " %s \n", filename);
        return NULL;
    }
    
    cur_bloom = counting_bloom_init(capacity, error_rate, 0);
    cur_bloom->bitmap = new_bitmap(fd, cur_bloom->num_bytes);
    cur_bloom->header = (counting_bloom_header_t *)(cur_bloom->bitmap->array);
    return cur_bloom;
}

/****
 *
 * Adds an element to the counting bloom filter
 *
 * This function adds a string element to the counting bloom filter by
 * computing hash values and incrementing the corresponding counters in
 * the bitmap. The element count is also incremented.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter structure
 *   s - String element to add to the filter
 *   len - Length of the string element in bytes
 *
 * Returns:
 *   Always returns 0
 *
 ****/
int counting_bloom_add(counting_bloom_t *bloom, const char *s, size_t len)
{
    unsigned int index, i, offset;
    unsigned int *hashes = bloom->hashes;
    
    hash_func(bloom, s, len, hashes);
    
    for (i = 0; i < bloom->nfuncs; i++) {
        offset = i * bloom->counts_per_func;
        index = hashes[i] + offset;
        bitmap_increment(bloom->bitmap, index, bloom->offset);
    }
    bloom->header->count++;
    
    return 0;
}

/****
 *
 * Removes an element from the counting bloom filter
 *
 * This function removes a string element from the counting bloom filter
 * by computing hash values and decrementing the corresponding counters
 * in the bitmap. The element count is also decremented.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter structure
 *   s - String element to remove from the filter
 *   len - Length of the string element in bytes
 *
 * Returns:
 *   Always returns 0
 *
 ****/
int counting_bloom_remove(counting_bloom_t *bloom, const char *s, size_t len)
{
    unsigned int index, i, offset;
    unsigned int *hashes = bloom->hashes;
    
    hash_func(bloom, s, len, hashes);
    
    for (i = 0; i < bloom->nfuncs; i++) {
        offset = i * bloom->counts_per_func;
        index = hashes[i] + offset;
        bitmap_decrement(bloom->bitmap, index, bloom->offset);
    }
    bloom->header->count--;
    
    return 0;
}

/****
 *
 * Checks if an element exists in the counting bloom filter
 *
 * This function tests whether a string element is present in the counting
 * bloom filter by computing hash values and checking if all corresponding
 * counters in the bitmap are non-zero.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter structure
 *   s - String element to check for presence in the filter
 *   len - Length of the string element in bytes
 *
 * Returns:
 *   1 if the element is probably in the filter, 0 if definitely not in the filter
 *
 ****/
int counting_bloom_check(counting_bloom_t *bloom, const char *s, size_t len)
{
    unsigned int index, i, offset;
    unsigned int *hashes = bloom->hashes;
    
    hash_func(bloom, s, len, hashes);
    
    for (i = 0; i < bloom->nfuncs; i++) {
        offset = i * bloom->counts_per_func;
        index = hashes[i] + offset;
        if (!(bitmap_check(bloom->bitmap, index, bloom->offset))) {
            return 0;
        }
    }
    return 1;
}

/****
 *
 * Frees a scaling bloom filter structure and all associated memory
 *
 * This function deallocates all memory associated with a scaling bloom
 * filter, including all individual bloom filters in the array, the bitmap,
 * and the scaling structure itself.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure to free
 *
 * Returns:
 *   Always returns 0
 *
 ****/
int free_scaling_bloom(scaling_bloom_t *bloom)
{
    int i;
    for (i = bloom->num_blooms - 1; i >= 0; i--) {
        free(bloom->blooms[i]->hashes);
        bloom->blooms[i]->hashes = NULL;
        free(bloom->blooms[i]);
        bloom->blooms[i] = NULL;
    }
    free(bloom->blooms);
    free_bitmap(bloom->bitmap);
    free(bloom);
    return 0;
}

/****
 *
 * Creates a new counting bloom filter as part of a scaling bloom filter
 *
 * This function creates a new counting bloom filter that becomes part of
 * a scaling bloom filter. It adjusts the error rate using a tightening
 * ratio and resizes the backing bitmap to accommodate the new filter.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *
 * Returns:
 *   Pointer to the new counting bloom filter on success, NULL on error
 *
 ****/
counting_bloom_t *new_counting_bloom_from_scale(scaling_bloom_t *bloom)
{
    int i;
    long offset;
    double error_rate;
    counting_bloom_t *cur_bloom;
    
    error_rate = bloom->error_rate * (pow(ERROR_TIGHTENING_RATIO, bloom->num_blooms + 1));
    
    if ((bloom->blooms = realloc(bloom->blooms, (bloom->num_blooms + 1) * sizeof(counting_bloom_t *))) == NULL) {
        fprintf(stderr, "Error, could not realloc a new bloom filter\n");
        return NULL;
    }
    
    cur_bloom = counting_bloom_init(bloom->capacity, error_rate, bloom->num_bytes);
    bloom->blooms[bloom->num_blooms] = cur_bloom;
    
    bloom->bitmap = bitmap_resize(bloom->bitmap, bloom->num_bytes, bloom->num_bytes + cur_bloom->num_bytes);
    
    /* reset header pointer, as mmap may have moved */
    bloom->header = (scaling_bloom_header_t *) bloom->bitmap->array;
    
    /* Set the pointers for these header structs to the right location since mmap may have moved */
    bloom->num_blooms++;
    for (i = 0; i < bloom->num_blooms; i++) {
        offset = bloom->blooms[i]->offset - sizeof(counting_bloom_header_t);
        bloom->blooms[i]->header = (counting_bloom_header_t *) (bloom->bitmap->array + offset);
    }
    
    bloom->num_bytes += cur_bloom->num_bytes;
    cur_bloom->bitmap = bloom->bitmap;
    
    return cur_bloom;
}

/****
 *
 * Creates a counting bloom filter from an existing file
 *
 * This function loads a counting bloom filter from a file that was
 * previously created with the same capacity and error rate parameters.
 * It validates that the file size matches the expected size.
 *
 * Arguments:
 *   capacity - Expected maximum number of elements the filter should hold
 *   error_rate - Expected false positive probability (0.0 to 1.0)
 *   filename - Path to the file containing the bloom filter data
 *
 * Returns:
 *   Pointer to the counting bloom filter on success, NULL on error
 *
 ****/
counting_bloom_t *new_counting_bloom_from_file(unsigned int capacity, double error_rate, const char *filename)
{
    int fd;
    off_t size;
    
    counting_bloom_t *bloom;
    
    if ((fd = open(filename, O_RDWR, (mode_t)0600)) < 0) {
        fprintf(stderr, "Error, Could not open file %s: %s\n", filename, strerror(errno));
        return NULL;
    }
    if ((size = lseek(fd, 0, SEEK_END)) < 0) {
        perror("Error, calling lseek() to tell file size");
        close(fd);
        return NULL;
    }
    if (size == 0) {
        fprintf(stderr, "Error, File size zero\n");
    }
    
    bloom = counting_bloom_init(capacity, error_rate, 0);
    
    if (size != bloom->num_bytes) {
        free_counting_bloom(bloom);
        fprintf(stderr, "Error, Actual filesize and expected filesize are not equal\n");
        return NULL;
    }
    if ((bloom->bitmap = new_bitmap(fd, size)) == NULL) {
        fprintf(stderr, "Error, Could not create bitmap with file\n");
        free_counting_bloom(bloom);
        return NULL;
    }
    
    bloom->header = (counting_bloom_header_t *)(bloom->bitmap->array);
    
    return bloom;
}

/****
 *
 * Clears sequence numbers for scaling bloom filter synchronization
 *
 * This function manages sequence numbers used for synchronizing changes
 * between memory and disk. It clears the disk sequence number if set,
 * flushes changes to disk, and returns the current memory sequence number.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *
 * Returns:
 *   The previous memory sequence number before clearing
 *
 ****/
uint64_t scaling_bloom_clear_seqnums(scaling_bloom_t *bloom)
{
    uint64_t seqnum;
    
    if (bloom->header->disk_seqnum != 0) {
        // disk_seqnum cleared on disk before any other changes
        bloom->header->disk_seqnum = 0;
        bitmap_flush(bloom->bitmap);
    }
    seqnum = bloom->header->mem_seqnum;
    bloom->header->mem_seqnum = 0;
    return seqnum;
}

/****
 *
 * Adds an element to the scaling bloom filter with ID tracking
 *
 * This function adds a string element to the scaling bloom filter,
 * finding the appropriate sub-filter based on the ID. If the current
 * filter is at capacity, it creates a new sub-filter to maintain
 * the scaling property.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *   s - String element to add to the filter
 *   len - Length of the string element in bytes
 *   id - Unique identifier associated with the element
 *
 * Returns:
 *   Always returns 1
 *
 ****/
int scaling_bloom_add(scaling_bloom_t *bloom, const char *s, size_t len, uint64_t id)
{
    int i;
    uint64_t seqnum;
    
    counting_bloom_t *cur_bloom = NULL;
    for (i = bloom->num_blooms - 1; i >= 0; i--) {
        cur_bloom = bloom->blooms[i];
        if (id >= cur_bloom->header->id) {
            break;
        }
    }
    
    seqnum = scaling_bloom_clear_seqnums(bloom);
    
    if ((id > bloom->header->max_id) && (cur_bloom->header->count >= cur_bloom->capacity - 1)) {
        cur_bloom = new_counting_bloom_from_scale(bloom);
        cur_bloom->header->count = 0;
        cur_bloom->header->id = bloom->header->max_id + 1;
    }
    if (bloom->header->max_id < id) {
        bloom->header->max_id = id;
    }
    counting_bloom_add(cur_bloom, s, len);
    
    bloom->header->mem_seqnum = seqnum + 1;
    
    return 1;
}

/****
 *
 * Removes an element from the scaling bloom filter with ID tracking
 *
 * This function removes a string element from the scaling bloom filter,
 * finding the appropriate sub-filter based on the ID. It searches through
 * the sub-filters in reverse order to find the correct one.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *   s - String element to remove from the filter
 *   len - Length of the string element in bytes
 *   id - Unique identifier associated with the element
 *
 * Returns:
 *   1 if the element was found and removed, 0 if not found
 *
 ****/
int scaling_bloom_remove(scaling_bloom_t *bloom, const char *s, size_t len, uint64_t id)
{
    counting_bloom_t *cur_bloom;
    int i;
    uint64_t seqnum;
    
    for (i = bloom->num_blooms - 1; i >= 0; i--) {
        cur_bloom = bloom->blooms[i];
        if (id >= cur_bloom->header->id) {
            seqnum = scaling_bloom_clear_seqnums(bloom);
            
            counting_bloom_remove(cur_bloom, s, len);
            
            bloom->header->mem_seqnum = seqnum + 1;
            return 1;
        }
    }
    return 0;
}

/****
 *
 * Checks if an element exists in the scaling bloom filter
 *
 * This function tests whether a string element is present in any of the
 * sub-filters within the scaling bloom filter. It searches through all
 * sub-filters in reverse order.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *   s - String element to check for presence in the filter
 *   len - Length of the string element in bytes
 *
 * Returns:
 *   1 if the element is probably in the filter, 0 if definitely not in the filter
 *
 ****/
int scaling_bloom_check(scaling_bloom_t *bloom, const char *s, size_t len)
{
    int i;
    counting_bloom_t *cur_bloom;
    for (i = bloom->num_blooms - 1; i >= 0; i--) {
        cur_bloom = bloom->blooms[i];
        if (counting_bloom_check(cur_bloom, s, len)) {
            return 1;
        }
    }
    return 0;
}

/****
 *
 * Performs atomic check-and-add operation on scaling bloom filter
 *
 * This function combines checking for element existence and adding it
 * if not present, avoiding duplicate hash computation. It first checks
 * all sub-filters for the element, and if not found, adds it to the
 * appropriate sub-filter.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *   s - String element to check and potentially add
 *   len - Length of the string element in bytes
 *   id - Unique identifier associated with the element
 *
 * Returns:
 *   1 if the element already existed, 0 if it was newly added
 *
 ****/
int scaling_bloom_check_add(scaling_bloom_t *bloom, const char *s, size_t len, uint64_t id)
{
    int i, found = 0;
    counting_bloom_t *cur_bloom = NULL;
    
    /* First check if item exists in any bloom filter */
    for (i = bloom->num_blooms - 1; i >= 0; i--) {
        cur_bloom = bloom->blooms[i];
        if (counting_bloom_check(cur_bloom, s, len)) {
            return 1; /* Already exists */
        }
        /* Find the appropriate bloom filter for adding */
        if (!found && id >= cur_bloom->header->id) {
            found = 1;
        }
    }
    
    /* Item doesn't exist, so add it */
    uint64_t seqnum = scaling_bloom_clear_seqnums(bloom);
    
    /* Use the appropriate bloom filter or create new one if needed */
    if (!found) {
        cur_bloom = bloom->blooms[bloom->num_blooms - 1];
    }
    
    if ((id > bloom->header->max_id) && (cur_bloom->header->count >= cur_bloom->capacity - 1)) {
        cur_bloom = new_counting_bloom_from_scale(bloom);
        cur_bloom->header->count = 0;
        cur_bloom->header->id = bloom->header->max_id + 1;
    }
    if (bloom->header->max_id < id) {
        bloom->header->max_id = id;
    }
    counting_bloom_add(cur_bloom, s, len);
    
    bloom->header->mem_seqnum = seqnum + 1;
    
    return 0; /* New item added */
}

/****
 *
 * Flushes scaling bloom filter changes to disk storage
 *
 * This function synchronizes the scaling bloom filter with its backing
 * file on disk. It ensures all changes are written before updating the
 * disk sequence number for proper synchronization.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure to flush
 *
 * Returns:
 *   0 on success, -1 on error
 *
 ****/
int scaling_bloom_flush(scaling_bloom_t *bloom)
{
    if (bitmap_flush(bloom->bitmap) != 0) {
        return -1;
    }
    // all changes written to disk before disk_seqnum set
    if (bloom->header->disk_seqnum == 0) {
        bloom->header->disk_seqnum = bloom->header->mem_seqnum;
        return bitmap_flush(bloom->bitmap);
    }
    return 0;
}

/****
 *
 * Returns the current memory sequence number of the scaling bloom filter
 *
 * This function provides access to the memory sequence number used for
 * tracking changes to the scaling bloom filter in memory.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *
 * Returns:
 *   Current memory sequence number
 *
 ****/
uint64_t scaling_bloom_mem_seqnum(scaling_bloom_t *bloom)
{
    return bloom->header->mem_seqnum;
}

/****
 *
 * Returns the current disk sequence number of the scaling bloom filter
 *
 * This function provides access to the disk sequence number used for
 * tracking changes to the scaling bloom filter that have been persisted
 * to disk storage.
 *
 * Arguments:
 *   bloom - Pointer to the scaling bloom filter structure
 *
 * Returns:
 *   Current disk sequence number
 *
 ****/
uint64_t scaling_bloom_disk_seqnum(scaling_bloom_t *bloom)
{
    return bloom->header->disk_seqnum;
}

/****
 *
 * Initializes a scaling bloom filter with specified parameters
 *
 * This function creates and initializes a scaling bloom filter structure
 * with the given capacity and error rate. It sets up the initial bitmap
 * and header structure for the scaling filter.
 *
 * Arguments:
 *   capacity - Maximum number of elements each sub-filter should hold
 *   error_rate - Desired false positive probability (0.0 to 1.0)
 *   filename - Path to the file that will back the bloom filter
 *   fd - File descriptor for the backing file
 *
 * Returns:
 *   Pointer to the initialized scaling bloom filter on success, NULL on error
 *
 ****/
scaling_bloom_t *scaling_bloom_init(unsigned int capacity, double error_rate, const char *filename, int fd)
{
    scaling_bloom_t *bloom;
    
    if ((bloom = malloc(sizeof(scaling_bloom_t))) == NULL) {
        return NULL;
    }
    if ((bloom->bitmap = new_bitmap(fd, sizeof(scaling_bloom_header_t))) == NULL) {
        fprintf(stderr, "Error, Could not create bitmap with file\n");
        free_scaling_bloom(bloom);
        return NULL;
    }
    
    bloom->header = (scaling_bloom_header_t *) bloom->bitmap->array;
    bloom->capacity = capacity;
    bloom->error_rate = error_rate;
    bloom->num_blooms = 0;
    bloom->num_bytes = sizeof(scaling_bloom_header_t);
    bloom->fd = fd;
    bloom->blooms = NULL;
    
    return bloom;
}

/****
 *
 * Creates a new scaling bloom filter backed by a file
 *
 * This function creates a new scaling bloom filter with the specified
 * capacity and error rate, backed by a file for persistent storage.
 * It initializes the filter with one sub-filter and sets up the
 * initial sequence numbers.
 *
 * Arguments:
 *   capacity - Maximum number of elements each sub-filter should hold
 *   error_rate - Desired false positive probability (0.0 to 1.0)
 *   filename - Path to the file that will back the bloom filter
 *
 * Returns:
 *   Pointer to the new scaling bloom filter on success, NULL on error
 *
 ****/
scaling_bloom_t *new_scaling_bloom(unsigned int capacity, double error_rate, const char *filename)
{

    scaling_bloom_t *bloom;
    counting_bloom_t *cur_bloom;
    int fd;
    
    if ((fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600)) < 0) {
        perror("Error, Opening File Failed");
        fprintf(stderr, " %s \n", filename);
        return NULL;
    }
    
    bloom = scaling_bloom_init(capacity, error_rate, filename, fd);
    
    if (!(cur_bloom = new_counting_bloom_from_scale(bloom))) {
        fprintf(stderr, "Error, Could not create counting bloom\n");
        free_scaling_bloom(bloom);
        return NULL;
    }
    cur_bloom->header->count = 0;
    cur_bloom->header->id = 0;
    
    bloom->header->mem_seqnum = 1;
    return bloom;
}

/****
 *
 * Creates a scaling bloom filter from an existing file
 *
 * This function loads a scaling bloom filter from a file that was
 * previously created. It reconstructs all sub-filters from the file
 * data and validates that the file size matches the expected size.
 *
 * Arguments:
 *   capacity - Expected maximum number of elements each sub-filter should hold
 *   error_rate - Expected false positive probability (0.0 to 1.0)
 *   filename - Path to the file containing the scaling bloom filter data
 *
 * Returns:
 *   Pointer to the scaling bloom filter on success, NULL on error
 *
 ****/
scaling_bloom_t *new_scaling_bloom_from_file(unsigned int capacity, double error_rate, const char *filename)
{
    int fd;
    off_t size;
    
    scaling_bloom_t *bloom;
    counting_bloom_t *cur_bloom;
    
    if ((fd = open(filename, O_RDWR, (mode_t)0600)) < 0) {
        fprintf(stderr, "Error, Could not open file %s: %s\n", filename, strerror(errno));
        return NULL;
    }
    if ((size = lseek(fd, 0, SEEK_END)) < 0) {
        perror("Error, calling lseek() to tell file size");
        close(fd);
        return NULL;
    }
    if (size == 0) {
        fprintf(stderr, "Error, File size zero\n");
    }
    
    bloom = scaling_bloom_init(capacity, error_rate, filename, fd);
    
    size -= sizeof(scaling_bloom_header_t);
    while (size) {
        cur_bloom = new_counting_bloom_from_scale(bloom);
        // leave count and id as they were set in the file
        size -= cur_bloom->num_bytes;
        if (size < 0) {
            free_scaling_bloom(bloom);
            fprintf(stderr, "Error, Actual filesize and expected filesize are not equal\n");
            return NULL;
        }
    }
    return bloom;
}
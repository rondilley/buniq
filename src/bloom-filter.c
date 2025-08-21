/*****
 *
 * Copyright (c) 2013-2025, Ron Dilley
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
 *****/

/****
 *
 * includes
 *.
 ****/

#include "bloom-filter.h"

/****
 *
 * local variables
 *
 ****/

/****
 *
 * global variables
 *
 ****/

/****
 *
 * external variables
 *
 ****/

extern char **environ;
extern int quit;
extern int reload;
extern Config_t *config;

/****
 *
 * functions
 *
 ****/

//bool f;         // conditional flag
//unsigned int m; // the bit mask
//unsigned int w; // the word to modify:  if (f) w |= m; else w &= ~m; 
//w ^= (-f ^ w) & m;
// OR, for superscalar CPUs:
//w = (w & ~m) | (-f & m);

/****
 *
 * Test if a bit is set in a byte buffer and optionally set it
 *
 * Tests whether a specific bit is set in a byte-aligned buffer.
 * If the bit is not set and set_bit is true, the bit will be set.
 * This function is used internally by the bloom filter for bit manipulation.
 *
 * Arguments:
 *   buf - Pointer to the byte buffer containing the bit array
 *   x - Bit position to test (0-based index)
 *   set_bit - Flag indicating whether to set the bit if not already set
 *
 * Returns:
 *   1 if the bit was already set, 0 if the bit was not set
 *
 ****/
inline static int test_bit_set_bit(unsigned char * buf, size_t x, int set_bit)
{
  size_t byte_offset = x >> 3;
  unsigned char c = buf[byte_offset];        // expensive memory access
  unsigned int mask = 1 << (x % 8);

  if (c & mask) {
    return 1;
  } else {
    if (set_bit) {
      buf[byte_offset] = c | mask;
    }
    return 0;
  }
}

/****
 *
 * Test if a bit is set in a 64-bit word buffer and set it
 *
 * Tests whether a specific bit is set in a 64-bit word-aligned buffer.
 * If the bit is not set, it will be set automatically. This function
 * is optimized for 64-bit operations and always sets the bit if not present.
 *
 * Arguments:
 *   buf - Pointer to the 64-bit word buffer containing the bit array
 *   x - Bit position to test (0-based index)
 *
 * Returns:
 *   1 if the bit was already set, 0 if the bit was not set (and has now been set)
 *
 ****/
inline static int test_bit_set_bit_64( uint64_t *buf, size_t x )
{
  size_t qword = x >> 6;
  uint64_t c = buf[qword];        // expensive memory access
  uint64_t mask = 1ULL << (x % 64);

  if (c & mask)
    return 1;

  buf[qword] = c | mask;
  return 0;
}

/****
 *
 * Check if an element exists in the bloom filter and optionally add it
 *
 * This is the core function that performs both checking and adding operations
 * for the bloom filter. It uses MurmurHash3 to generate hash values and tests
 * multiple bit positions based on the configured number of hash functions.
 *
 * Arguments:
 *   bloom - Pointer to initialized bloom filter structure
 *   buffer - Pointer to data buffer containing the element to check/add
 *   len - Length of the data buffer in bytes
 *   add - Flag indicating whether to add the element (1) or just check (0)
 *
 * Returns:
 *   1 if element was already present (or collision occurred)
 *   0 if element was not present (and added if add=1)
 *   -1 if bloom filter is not initialized
 *
 ****/
inline static int bloom_check_add(struct bloom * bloom,
                           const void * buffer, int len, int add)
{
  uint64_t hash[2];
  if (bloom->ready EQ 0) {
    fprintf(stderr, "bloom at %p not initialized!\n", (void *)bloom);
    return -1;
  }

  int hits = 0;
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash );
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;

  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i*b) % bloom->bits;
    if (test_bit_set_bit(bloom->bf, x, add)) {
      hits++;
    } else if (!add) {
      // Don't care about the presence of all the bits. Just our own.
      return 0;
    }
  }

  if (hits EQ bloom->hashes) {
    return 1;                // 1 == element already in (or collision)
  }

  return 0;
}

/****
 *
 * Check if an element exists in 64-bit bloom filter and add if not present
 *
 * This function performs a two-pass operation on a 64-bit optimized bloom filter.
 * First pass checks all required bits without modification. If all bits are set,
 * the element is considered present. If not all bits are set, a second pass
 * sets all the required bits to add the element.
 *
 * Arguments:
 *   bloom - Pointer to initialized bloom filter structure with 64-bit buffer
 *   buffer - Pointer to data buffer containing the element to check/add
 *   len - Length of the data buffer in bytes
 *
 * Returns:
 *   1 if element was already present (or collision occurred)
 *   0 if element was not present and has been added
 *
 ****/
inline int bloom_check_add_64(struct bloom * bloom, const void * buffer, int len )
{
  uint64_t hash[2];

  int hits = 0;
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash );
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;

  /* First pass: check all bits without modifying */
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i*b) % bloom->bits;
    size_t qword = x >> 6;
    uint64_t mask = 1ULL << (x % 64);
    if (bloom->bf64[qword] & mask) {
      hits++;
    }
  }

  if (hits EQ bloom->hashes) {
    return 1;                // 1 == element already in (or collision)
  }

  /* Second pass: set all bits since element is new */
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i*b) % bloom->bits;
    size_t qword = x >> 6;
    uint64_t mask = 1ULL << (x % 64);
    bloom->bf64[qword] |= mask;
  }

  return 0;                  // new element added
}

/****
 *
 * Optimized check and add for 64-bit bloom filter with improved performance
 *
 * This is an optimized version of bloom_check_add_64 that reduces memory access
 * overhead by caching 64-bit words during the checking phase. Like the standard
 * version, it performs a two-pass operation but with better cache locality.
 *
 * Arguments:
 *   bloom - Pointer to initialized bloom filter structure with 64-bit buffer
 *   buffer - Pointer to data buffer containing the element to check/add
 *   len - Length of the data buffer in bytes
 *
 * Returns:
 *   1 if element was already present (or collision occurred)
 *   0 if element was not present and has been added
 *
 ****/
inline int bloom_check_add_64_optimized(struct bloom * bloom, const void * buffer, int len )
{
  uint64_t hash[2];

  int hits = 0;
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash );
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;

  /* Check all bits first */
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i*b) % bloom->bits;
    size_t qword = x >> 6;
    uint64_t c = bloom->bf64[qword];
    uint64_t mask = 1ULL << (x % 64);
    if (c & mask) {
      hits++;
    }
  }

  if (hits EQ bloom->hashes) {
    return 1;                // element already present
  }

  /* Not all bits set, so set them all and return 0 (new element) */
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i*b) % bloom->bits;
    size_t qword = x >> 6;
    bloom->bf64[qword] |= (1ULL << (x % 64));
  }

  return 0;                  // new element added
}

/** ***************************************************************************
 * Initialize the bloom filter for use.
 *
 * The filter is initialized with a bit field and number of hash functions
 * according to the computations from the wikipedia entry:
 *     http://en.wikipedia.org/wiki/Bloom_filter
 *
 * Optimal number of bits is:
 *     bits = (entries * ln(error)) / ln(2)^2
 *
 * Optimal number of hash functions is:
 *     hashes = bpe * ln(2)
 *
 * Parameters:
 * -----------
 *     bloom   - Pointer to an allocated struct bloom (see above).
 *     entries - The expected number of entries which will be inserted.
 *               Must be at least 1000 (in practice, likely much larger).
 *     error   - Probability of collision (as long as entries are not
 *               exceeded).
 *
 * Return:
 * -------
 *     0 - on success
 *     1 - on failure
 *
 */

int bloom_init(struct bloom * bloom, size_t entries, double error)
{
  bloom->ready = 0;

  /* Validate input parameters */
  if (entries < 1000 || entries > SIZE_MAX / 64) {
    return 1; /* entries too small or too large (potential overflow) */
  }
  
  if (error <= 0.0 || error >= 1.0) {
    return 1; /* error rate must be between 0 and 1 */
  }

  bloom->entries = entries;
  bloom->error = error;

  double num = log(bloom->error);
  double denom = 0.480453013918201; // ln(2)^2
  bloom->bpe = -(num / denom);

  double dentries = (double)entries;
  bloom->bits = (size_t)(dentries * bloom->bpe);

  if (bloom->bits % 8) {
    bloom->bytes = (bloom->bits / 8) + 1;
  } else {
    bloom->bytes = bloom->bits / 8;
  }

  bloom->hashes = (int)ceil(0.693147180559945 * bloom->bpe);  // ln(2)

  bloom->bf = (unsigned char *)XMALLOC( bloom->bytes );
  if (bloom->bf EQ NULL) {
    return 1;
  }

  bloom->ready = 1;
  return 0;
}

/****
 *
 * Initialize a 64-bit optimized bloom filter for use
 *
 * This function initializes a bloom filter optimized for 64-bit operations.
 * It uses the same mathematical calculations as the standard bloom_init but
 * allocates memory in 64-bit word chunks for better performance. The filter
 * is initialized with optimal parameters based on expected entries and error rate.
 *
 * Arguments:
 *   bloom - Pointer to an allocated struct bloom to initialize
 *   entries - Expected number of entries (must be at least 1000)
 *   error - Desired false positive probability (between 0.0 and 1.0)
 *
 * Returns:
 *   0 on success
 *   1 on failure (invalid parameters or memory allocation failure)
 *
 ****/
int bloom_init_64(struct bloom * bloom, size_t entries, double error)
{
  bloom->ready = 0;

  /* Validate input parameters */
  if (entries < 1000 || entries > SIZE_MAX / 64) {
    return 1; /* entries too small or too large (potential overflow) */
  }
  
  if (error <= 0.0 || error >= 1.0) {
    return 1; /* error rate must be between 0 and 1 */
  }

  bloom->entries = entries;
  bloom->error = error;

  double num = log(bloom->error);
  double denom = 0.480453013918201; // ln(2)^2
  bloom->bpe = -(num / denom);

  double dentries = (double)entries;
  bloom->bits = (size_t)(dentries * bloom->bpe);

  if (bloom->bits % 64) {
    bloom->qwords = (bloom->bits / 64) + 1;
  } else {
    bloom->qwords = bloom->bits / 64;
  }

  bloom->hashes = (int)ceil(0.693147180559945 * bloom->bpe);  // ln(2)

  bloom->bytes = bloom->qwords * sizeof(uint64_t);  // Set bytes field for 64-bit version

  if ( ( bloom->bf64 = (uint64_t *)XMALLOC( bloom->bytes ) ) EQ NULL )
    return 1;

  bloom->ready = 1;
  return 0;
}

/** ***************************************************************************
 * Check if the given element is in the bloom filter. Remember this may
 * return false positive if a collision occurred.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *     buffer - Pointer to buffer containing element to check.
 *     len    - Size of 'buffer'.
 *
 * Return:
 * -------
 *     0 - element is not present
 *     1 - element is present (or false positive due to collision)
 *    -1 - bloom not initialized
 *
 */
inline static int bloom_check(struct bloom * bloom, const void * buffer, int len)
{
  return bloom_check_add(bloom, buffer, len, 0);
}

/** ***************************************************************************
 * Add the given element to the bloom filter.
 * The return code indicates if the element (or a collision) was already in,
 * so for the common check+add use case, no need to call check separately.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *     buffer - Pointer to buffer containing element to add.
 *     len    - Size of 'buffer'.
 *
 * Return:
 * -------
 *     0 - element was not present and was added
 *     1 - element (or a collision) had already been added previously
 *    -1 - bloom not initialized
 *
 */
inline static int bloom_add(struct bloom * bloom, const void * buffer, int len)
{
  return bloom_check_add(bloom, buffer, len, 1);
}

/****
 *
 * Print diagnostic information about the bloom filter to stderr
 *
 * This function outputs detailed information about the bloom filter's
 * configuration and state to stderr for debugging purposes. It displays
 * memory addresses, capacity, error rate, bit count, and other parameters.
 *
 * Arguments:
 *   bloom - Pointer to initialized bloom filter structure
 *
 * Returns:
 *   None (void function)
 *
 ****/
void bloom_print(struct bloom * bloom)
{
  fprintf(stderr, "bloom at %p\n", (void *)bloom);
  fprintf(stderr, " ->entries = %ld\n", bloom->entries);
  fprintf(stderr, " ->error = %lf\n", bloom->error);
  fprintf(stderr, " ->bits = %ld\n", bloom->bits);
  fprintf(stderr, " ->bits per elem = %f\n", bloom->bpe);
  fprintf(stderr, " ->bytes = %ld\n", bloom->bytes);
  fprintf(stderr, " ->hash functions = %d\n", bloom->hashes);
}

/** ***************************************************************************
 * Deallocate internal storage.
 *
 * Upon return, the bloom struct is no longer usable. You may call bloom_init
 * again on the same struct to reinitialize it again.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *
 * Return: none
 *
 */
void bloom_free(struct bloom * bloom)
{
  if ( bloom->bf != NULL )
    XFREE(bloom->bf);
  else if ( bloom->bf64 != NULL )
    XFREE( bloom->bf64 );

  bloom->ready = 0;
}

/** ***************************************************************************
 * Erase internal storage.
 *
 * Erases all elements. Upon return, the bloom struct returns to its initial
 * (initialized) state.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *
 * Return:
 *     0 - on success
 *     1 - on failure
 *
 */
int bloom_reset(struct bloom * bloom)
{
  if (!bloom->ready) return 1;
  if ( bloom->bf != NULL )
    memset(bloom->bf, 0, bloom->bytes);
  else if ( bloom->bf64 != NULL )
    memset( bloom->bf64, 0, bloom->qwords * sizeof( uint64_t ));
  return 0;
}

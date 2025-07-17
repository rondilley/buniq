/*****
 *
 * Description: Counting Bloom Filter Implementation
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

#include "counting-bloom.h"

/****
 *
 * Get the 4-bit counter value at the specified position in the counter array
 *
 * Each byte in the counter array holds two 4-bit counters. This function
 * extracts the appropriate 4-bit counter value from the packed byte array.
 *
 * Arguments:
 *   counts - Pointer to the packed counter array
 *   pos - Position index of the counter to retrieve
 *
 * Returns:
 *   The 4-bit counter value (0-15) at the specified position
 *
 ****/
static inline uint8_t get_counter(uint8_t *counts, size_t pos) {
  size_t byte_pos = pos / 2;
  if (pos % 2 == 0) {
    return counts[byte_pos] & 0x0F;  /* Lower 4 bits */
  } else {
    return (counts[byte_pos] & 0xF0) >> 4;  /* Upper 4 bits */
  }
}

/****
 *
 * Set the 4-bit counter value at the specified position in the counter array
 *
 * Each byte in the counter array holds two 4-bit counters. This function
 * sets the appropriate 4-bit counter value in the packed byte array while
 * preserving the other counter in the same byte.
 *
 * Arguments:
 *   counts - Pointer to the packed counter array
 *   pos - Position index of the counter to set
 *   value - The 4-bit counter value (0-15) to set
 *
 * Returns:
 *   None
 *
 ****/
static inline void set_counter(uint8_t *counts, size_t pos, uint8_t value) {
  size_t byte_pos = pos / 2;
  if (pos % 2 == 0) {
    counts[byte_pos] = (counts[byte_pos] & 0xF0) | (value & 0x0F);
  } else {
    counts[byte_pos] = (counts[byte_pos] & 0x0F) | ((value & 0x0F) << 4);
  }
}

/****
 *
 * Increment the 4-bit counter at the specified position
 *
 * Safely increments the counter value by 1, ensuring it does not exceed
 * the maximum value of 15 for a 4-bit counter. If the counter is already
 * at maximum value, it remains unchanged.
 *
 * Arguments:
 *   counts - Pointer to the packed counter array
 *   pos - Position index of the counter to increment
 *
 * Returns:
 *   None
 *
 ****/
static inline void increment_counter(uint8_t *counts, size_t pos) {
  uint8_t current = get_counter(counts, pos);
  if (current < 15) {  /* Max value for 4-bit counter */
    set_counter(counts, pos, current + 1);
  }
}

/****
 *
 * Initialize an enhanced counting bloom filter with specified parameters
 *
 * Calculates optimal bloom filter parameters based on expected number of
 * entries and desired false positive error rate. Allocates memory for the
 * counter array and initializes all statistics to zero.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter structure to initialize
 *   entries - Expected number of unique entries to be inserted
 *   error - Desired false positive error rate (0.0 < error < 1.0)
 *
 * Returns:
 *   0 on success, 1 on error (invalid parameters or memory allocation failure)
 *
 ****/
int enhanced_counting_bloom_init(struct enhanced_counting_bloom *bloom, size_t entries, double error) {
  bloom->ready = 0;
  
  /* Validate input parameters */
  if (entries < 1000 || entries > SIZE_MAX / 64) {
    return 1;
  }
  
  if (error <= 0.0 || error >= 1.0) {
    return 1;
  }
  
  bloom->entries = entries;
  bloom->error = error;
  
  /* Calculate optimal parameters */
  double num = log(bloom->error);
  double denom = 0.480453013918201; // ln(2)^2
  bloom->bpe = -(num / denom);
  
  double dentries = (double)entries;
  bloom->bits = (size_t)(dentries * bloom->bpe);
  bloom->counters = bloom->bits;
  
  bloom->hashes = (int)ceil(0.693147180559945 * bloom->bpe);  // ln(2)
  
  /* Allocate counter array (4-bit counters, so bits/2 bytes) */
  size_t counter_bytes = (bloom->counters + 1) / 2;
  bloom->counts = (uint8_t *)XMALLOC(counter_bytes);
  if (bloom->counts == NULL) {
    return 1;
  }
  
  /* Initialize statistics */
  bloom->total_insertions = 0;
  bloom->unique_insertions = 0;
  
  bloom->ready = 1;
  return 0;
}

/****
 *
 * Add an item to the counting bloom filter and return presence status
 *
 * Hashes the input buffer and increments all corresponding counters in the
 * filter. Checks if the item was already present before incrementing.
 * Updates insertion statistics.
 *
 * Arguments:
 *   bloom - Pointer to the initialized counting bloom filter
 *   buffer - Pointer to the data to add to the filter
 *   len - Length of the data in bytes
 *
 * Returns:
 *   0 if item was new (not previously present)
 *   1 if item was already present (or collision)
 *   -1 on error (filter not ready)
 *
 ****/
int enhanced_counting_bloom_add(struct enhanced_counting_bloom *bloom, const void *buffer, int len) {
  if (bloom->ready == 0) {
    return -1;
  }
  
  uint64_t hash[2];
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash);
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;
  
  /* Check if all counters are non-zero first */
  int all_present = 1;
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i * b) % bloom->counters;
    if (get_counter(bloom->counts, x) == 0) {
      all_present = 0;
      break;
    }
  }
  
  /* Increment all counters */
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i * b) % bloom->counters;
    increment_counter(bloom->counts, x);
  }
  
  bloom->total_insertions++;
  if (!all_present) {
    bloom->unique_insertions++;
    return 0;  /* New item */
  }
  
  return 1;  /* Existing item */
}

/****
 *
 * Check if an item is present in the counting bloom filter
 *
 * Hashes the input buffer and checks if all corresponding counters are
 * non-zero. Does not modify the filter state.
 *
 * Arguments:
 *   bloom - Pointer to the initialized counting bloom filter
 *   buffer - Pointer to the data to check
 *   len - Length of the data in bytes
 *
 * Returns:
 *   1 if item is present (or collision)
 *   0 if item is definitely not present
 *   -1 on error (filter not ready)
 *
 ****/
int enhanced_counting_bloom_check(struct enhanced_counting_bloom *bloom, const void *buffer, int len) {
  if (bloom->ready == 0) {
    return -1;
  }
  
  uint64_t hash[2];
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash);
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;
  
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i * b) % bloom->counters;
    if (get_counter(bloom->counts, x) == 0) {
      return 0;  /* Not present */
    }
  }
  
  return 1;  /* Present (or collision) */
}

/****
 *
 * Get the estimated count of an item in the counting bloom filter
 *
 * Hashes the input buffer and returns the minimum counter value among
 * all hash positions. This provides an estimate of how many times the
 * item has been inserted (subject to collisions).
 *
 * Arguments:
 *   bloom - Pointer to the initialized counting bloom filter
 *   buffer - Pointer to the data to get count for
 *   len - Length of the data in bytes
 *
 * Returns:
 *   Estimated count (0-15) of the item
 *   -1 on error (filter not ready)
 *
 ****/
int enhanced_counting_bloom_get_count(struct enhanced_counting_bloom *bloom, const void *buffer, int len) {
  if (bloom->ready == 0) {
    return -1;
  }
  
  uint64_t hash[2];
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash);
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;
  
  int min_count = 15;  /* Start with maximum possible value */
  
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i * b) % bloom->counters;
    int count = get_counter(bloom->counts, x);
    if (count < min_count) {
      min_count = count;
    }
  }
  
  return min_count;
}

/****
 *
 * Get item count and add item to the counting bloom filter atomically
 *
 * Hashes the input buffer, gets the minimum counter value (estimated count)
 * before incrementing, then increments all corresponding counters.
 * Updates insertion statistics.
 *
 * Arguments:
 *   bloom - Pointer to the initialized counting bloom filter
 *   buffer - Pointer to the data to check and add
 *   len - Length of the data in bytes
 *
 * Returns:
 *   Previous estimated count (0-15) of the item before incrementing
 *   -1 on error (filter not ready)
 *
 ****/
int enhanced_counting_bloom_check_add_count(struct enhanced_counting_bloom *bloom, const void *buffer, int len) {
  if (bloom->ready == 0) {
    return -1;
  }
  
  uint64_t hash[2];
  MurmurHash3_x64_128(buffer, len, 0x9747b28c, &hash);
  register uint64_t a = hash[0];
  register uint64_t b = hash[1];
  register uint64_t x;
  register uint64_t i;
  
  /* Get minimum count before incrementing */
  int min_count = 15;
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i * b) % bloom->counters;
    int count = get_counter(bloom->counts, x);
    if (count < min_count) {
      min_count = count;
    }
  }
  
  /* Increment all counters */
  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i * b) % bloom->counters;
    increment_counter(bloom->counts, x);
  }
  
  bloom->total_insertions++;
  if (min_count == 0) {
    bloom->unique_insertions++;
  }
  
  return min_count;
}

/****
 *
 * Print debugging information about the counting bloom filter
 *
 * Outputs the bloom filter's configuration parameters and statistics
 * to stderr for debugging and monitoring purposes.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter to print information about
 *
 * Returns:
 *   None
 *
 ****/
void enhanced_counting_bloom_print(struct enhanced_counting_bloom *bloom) {
  fprintf(stderr, "counting bloom at %p\n", (void *)bloom);
  fprintf(stderr, " ->entries = %ld\n", bloom->entries);
  fprintf(stderr, " ->error = %lf\n", bloom->error);
  fprintf(stderr, " ->bits = %ld\n", bloom->bits);
  fprintf(stderr, " ->counters = %ld\n", bloom->counters);
  fprintf(stderr, " ->bits per elem = %f\n", bloom->bpe);
  fprintf(stderr, " ->hash functions = %d\n", bloom->hashes);
  fprintf(stderr, " ->total insertions = %lu\n", bloom->total_insertions);
  fprintf(stderr, " ->unique insertions = %lu\n", bloom->unique_insertions);
}

/****
 *
 * Free memory allocated for the counting bloom filter
 *
 * Deallocates the counter array and marks the filter as not ready.
 * Should be called when the filter is no longer needed to prevent
 * memory leaks.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter to free
 *
 * Returns:
 *   None
 *
 ****/
void enhanced_counting_bloom_free(struct enhanced_counting_bloom *bloom) {
  if (bloom->counts != NULL) {
    XFREE(bloom->counts);
  }
  bloom->ready = 0;
}

/****
 *
 * Reset the counting bloom filter to empty state
 *
 * Clears all counters to zero and resets insertion statistics.
 * The filter configuration and allocated memory remain unchanged,
 * allowing reuse of the same filter structure.
 *
 * Arguments:
 *   bloom - Pointer to the counting bloom filter to reset
 *
 * Returns:
 *   0 on success, 1 on error (filter not ready)
 *
 ****/
int enhanced_counting_bloom_reset(struct enhanced_counting_bloom *bloom) {
  if (!bloom->ready) return 1;
  
  size_t counter_bytes = (bloom->counters + 1) / 2;
  memset(bloom->counts, 0, counter_bytes);
  bloom->total_insertions = 0;
  bloom->unique_insertions = 0;
  
  return 0;
}
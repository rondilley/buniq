/*****
*
* Description: bloom filter functions
*
* Copyright (c) 2008-2017, Ron Dilley
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

/****
*
* defines
*
****/

/****
*
* includes
*
****/

#include "bloom.h"

/****
*
* local variables
*
****/

/****
*
* external global variables
*
****/

extern Config_t *config;

/****
*
* functions
*
****/

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "murmur.h"
#include "bloom.h"

BloomFilter * bloom_filter_new(gsize filter_size, gsize num_hashes, BloomContains contains, gpointer user)
{
    BloomFilter	*bf;
    const gsize	bits_length = (filter_size + (CHAR_BIT * sizeof *bf->bits) - 1) / (CHAR_BIT * sizeof *bf->bits);
    const gsize	bits_size = bits_length * sizeof *bf->bits;

    if((bf = g_malloc(sizeof *bf + bits_size)) != NULL)
    {
        bf->m = filter_size;
        bf->k = num_hashes;
        bf->size = 0;
        bf->bits = (gulong *) (bf + 1);
        bf->bits_length = bits_length;
        bf->contains = contains != NULL ? contains : cb_contains_always;
        bf->user = user;
        memset(bf->bits, 0, bits_size);
    }
    return bf;
}

BloomFilter * bloom_filter_new_with_probability(gfloat prob, gsize num_elements, BloomContains contains, gpointer user)
{
    const gfloat	m = -(num_elements * logf(prob)) / powf(log(2.f), 2.f);
    const gfloat	k = logf(2.f) * m / num_elements;

    printf("computed bloom filter size %f -> %u bytes\n", m, (guint) (m + .5f) / 8);
    printf(" so m/n = %.1f\n", m / num_elements);
    printf(" which gives k=%f\n", k);

    return bloom_filter_new((gsize) (m + .5f), (guint) (k + 0.5f), contains, user);
}

void bloom_filter_destroy(BloomFilter *bf)
{
    g_free(bf);
}

size_t bloom_filter_num_bits(const BloomFilter *bf)
{
    return bf->m;
}

size_t bloom_filter_num_hashes(const BloomFilter *bf)
{
    return bf->k;
}

size_t bloom_filter_size(const BloomFilter *bf)
{
    return bf->size;
}

void bloom_filter_insert(BloomFilter *bf, const gchar *string, gssize string_length)
{
    const gsize	len = string_length > 0 ? string_length : strlen(string);
    gsize		i;

    /* Repeatedly hash the string, and set bits in the Bloom filter's bit array. */
    for(i = 0; i < bf->k; i++)
    {
        const guint32	hash = MurmurHash2(string, len, i);
        const gsize	pos = hash % bf->m;
        const gsize	slot = pos / (CHAR_BIT * sizeof *bf->bits);
        const gsize	bit = pos % (CHAR_BIT * sizeof *bf->bits);

        printf("hash(%s,%zu)=%u -> pos=%zu -> slot=%zu, bit=%zu\n", string, i, hash, pos, slot, bit);
        bf->bits[slot] |= 1UL << bit;
    }
    bf->size++;
}

gboolean bloom_filter_contains(const BloomFilter *bf, const gchar *string, gssize string_length)
{
    const gsize	len = string_length > 0 ? string_length : strlen(string);
    gsize		i;

    /* Check the Bloom filter, by hashing and checking bits. */
    for(i = 0; i < bf->k; i++)
    {
        const guint32	hash = MurmurHash2(string, len, i);
        const gsize	pos = hash % bf->m;
        const gsize	slot = pos / (CHAR_BIT * sizeof *bf->bits);
        const gsize	bit = pos % (CHAR_BIT * sizeof *bf->bits);

        /* If a bit is not set, the element is not contained, for sure. */
        if((bf->bits[slot] & (1UL << bit)) == 0)
            return FALSE;
    }
    /* Bit-checking says yes, call user's contains() function to make sure. */
    return bf->contains(bf->user, string, len);
}

// simpler option

/*
 *  Copyright (c) 2012-2017, Jyri J. Virkki
 *  All rights reserved.
 *
 *  This file is under BSD license. See LICENSE file.
 */

/*
 * Refer to bloom.h for documentation on the public interfaces.
 */

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bloom.h"
#include "murmurhash2.h"

#define MAKESTRING(n) STRING(n)
#define STRING(n) #n


inline static int test_bit_set_bit(unsigned char * buf,
                                   unsigned int x, int set_bit)
{
  unsigned int byte = x >> 3;
  unsigned char c = buf[byte];        // expensive memory access
  unsigned int mask = 1 << (x % 8);

  if (c & mask) {
    return 1;
  } else {
    if (set_bit) {
      buf[byte] = c | mask;
    }
    return 0;
  }
}


static int bloom_check_add(struct bloom * bloom,
                           const void * buffer, int len, int add)
{
  if (bloom->ready == 0) {
    printf("bloom at %p not initialized!\n", (void *)bloom);
    return -1;
  }

  int hits = 0;
  register unsigned int a = murmurhash2(buffer, len, 0x9747b28c);
  register unsigned int b = murmurhash2(buffer, len, a);
  register unsigned int x;
  register unsigned int i;

  for (i = 0; i < bloom->hashes; i++) {
    x = (a + i*b) % bloom->bits;
    if (test_bit_set_bit(bloom->bf, x, add)) {
      hits++;
    }
  }

  if (hits == bloom->hashes) {
    return 1;                // 1 == element already in (or collision)
  }

  return 0;
}


int bloom_init_size(struct bloom * bloom, int entries, double error,
                    unsigned int cache_size)
{
  return bloom_init(bloom, entries, error);
}


int bloom_init(struct bloom * bloom, int entries, double error)
{
  bloom->ready = 0;

  if (entries < 1000 || error == 0) {
    return 1;
  }

  bloom->entries = entries;
  bloom->error = error;

  double num = log(bloom->error);
  double denom = 0.480453013918201; // ln(2)^2
  bloom->bpe = -(num / denom);

  double dentries = (double)entries;
  bloom->bits = (int)(dentries * bloom->bpe);

  if (bloom->bits % 8) {
    bloom->bytes = (bloom->bits / 8) + 1;
  } else {
    bloom->bytes = bloom->bits / 8;
  }

  bloom->hashes = (int)ceil(0.693147180559945 * bloom->bpe);  // ln(2)

  bloom->bf = (unsigned char *)calloc(bloom->bytes, sizeof(unsigned char));
  if (bloom->bf == NULL) {
    return 1;
  }

  bloom->ready = 1;
  return 0;
}


int bloom_check(struct bloom * bloom, const void * buffer, int len)
{
  return bloom_check_add(bloom, buffer, len, 0);
}


int bloom_add(struct bloom * bloom, const void * buffer, int len)
{
  return bloom_check_add(bloom, buffer, len, 1);
}


void bloom_print(struct bloom * bloom)
{
  printf("bloom at %p\n", (void *)bloom);
  printf(" ->entries = %d\n", bloom->entries);
  printf(" ->error = %f\n", bloom->error);
  printf(" ->bits = %d\n", bloom->bits);
  printf(" ->bits per elem = %f\n", bloom->bpe);
  printf(" ->bytes = %d\n", bloom->bytes);
  printf(" ->hash functions = %d\n", bloom->hashes);
}


void bloom_free(struct bloom * bloom)
{
  if (bloom->ready) {
    free(bloom->bf);
  }
  bloom->ready = 0;
}


const char * bloom_version()
{
  return MAKESTRING(BLOOM_VERSION);
}

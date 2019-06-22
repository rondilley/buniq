/*****
 *
 * Copyright (c) 2013-2019, Ron Dilley
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   - Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   - Neither the name of Uberadmin/BaraCUDA/Nightingale nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****/

#ifndef BLOOM_FILTER_DOT_H
#define BLOOM_FILTER_DOT_H

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sysdep.h>

#ifndef __SYSDEP_H__
# error something is messed up
#endif

#include <common.h>
#include "util.h"
#include "mem.h"
#include "getopt.h"
#include "hash.h"
#include "md5.h"

/****
 *
 * consts & enums
 *
 ****/

/****
 *
 * typedefs & structs
 *
 ****/

struct bloom_hash {
    hash_function func;
    struct bloom_hash *next;
};

typedef struct bloom_filter {
    struct bloom_hash *func;
    void *bits;
    size_t size;
} bloom_t;

/****
 *
 * function prototypes
 *
 ****/


typedef unsigned int (*hash_function)(const void *data);


/* Creates a new bloom filter with no hash functions and size * 8 bits. */
bloom_t *bloom_create(size_t size);
/* Frees a bloom filter. */
void bloom_free(bloom_t *filter);
/* Adds a hashing function to the bloom filter. You should add all of the
 * functions you intend to use before you add any items. */
void bloom_add_hash(bloom_t *filter, hash_function func);
/* Adds an item to the bloom filter. */
void bloom_add(bloom_t *filter, const void *item);
/* Tests if an item is in the bloom filter.
 *
 * Returns false if the item has definitely not been added before. Returns true
 * if the item was probably added before. */
bool bloom_test(bloom_t *filter, const void *item);

#endif /* BLOOM_FILTER_DOT_H */

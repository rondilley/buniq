/*****
 *
 * Description: Counting Bloom Filter Headers
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

#ifndef COUNTING_BLOOM_DOT_H
#define COUNTING_BLOOM_DOT_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "../include/sysdep.h"
#include "../include/common.h"
#include <math.h>
#include "util.h"
#include "mem.h"
#include "murmur.h"

/* Enhanced counting bloom filter structure */
struct enhanced_counting_bloom {
  size_t entries;
  double error;
  size_t bits;
  size_t counters;
  int hashes;
  double bpe;
  uint8_t *counts;  /* Counter array (4-bit counters) */
  int ready;
  
  /* Statistics for counting */
  uint64_t total_insertions;
  uint64_t unique_insertions;
};

/* Function prototypes */
int enhanced_counting_bloom_init(struct enhanced_counting_bloom *bloom, size_t entries, double error);
int enhanced_counting_bloom_add(struct enhanced_counting_bloom *bloom, const void *buffer, int len);
int enhanced_counting_bloom_check(struct enhanced_counting_bloom *bloom, const void *buffer, int len);
int enhanced_counting_bloom_get_count(struct enhanced_counting_bloom *bloom, const void *buffer, int len);
int enhanced_counting_bloom_check_add_count(struct enhanced_counting_bloom *bloom, const void *buffer, int len);
void enhanced_counting_bloom_print(struct enhanced_counting_bloom *bloom);
void enhanced_counting_bloom_free(struct enhanced_counting_bloom *bloom);
int enhanced_counting_bloom_reset(struct enhanced_counting_bloom *bloom);

#endif /* COUNTING_BLOOM_DOT_H */
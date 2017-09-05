/*****
*
* Description: bloom filter headers
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

#ifndef BLOOM_DOT_H
#define BLOOM_DOT_H

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
#include "mem.h"

/****
*
* typedefs and enums
*
****/

struct BloomFilter {
        size_t		m;
        size_t		k;
        size_t		size;
        gulong		*bits;
        size_t		bits_length;
        BloomContains	contains;
};

/****
*
* function prototypes
*
****/

typedef struct BloomFilter BloomFilter;

/* A callback function for exactly determining if the string is contained.
 * For a "pure" probabilistic Bloom filter, use NULL. For our application,
 * we must be able to determine exact containment.
*/
typedef gboolean	(*BloomContains)(gpointer user, const gchar *string, gsize string_length);

extern BloomFilter *	bloom_filter_new(gsize filter_size, gsize num_hashes, BloomContains contains, gpointer user);
extern BloomFilter *	bloom_filter_new_with_probability(gfloat prob, gsize num_elements, BloomContains contains, gpointer user);
extern gsize		bloom_filter_num_bits(const BloomFilter *bf);
extern gsize		bloom_filter_num_hashes(const BloomFilter *bf);
extern gsize		bloom_filter_size(const BloomFilter *bf);
extern void		bloom_filter_insert(BloomFilter *bf, const gchar *string, gssize string_length);
extern gboolean		bloom_filter_contains(const BloomFilter *bf, const gchar *string, gssize string_length);

#endif /* end of BLOOM_DOT_H */


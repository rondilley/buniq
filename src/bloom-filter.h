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

#include "../include/sysdep.h"

#ifndef __SYSDEP_H__
# error something is messed up
#endif

#include "../include/common.h"
#include <math.h>
#include "util.h"
#include "mem.h"
#include "getopt.h"
#include "hash.h"
#include "md5.h"
#include "murmur.h"

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

/** ***************************************************************************
 * Structure to keep track of one bloom filter.  Caller needs to
 * allocate this and pass it to the functions below. First call for
 * every struct must be to bloom_init().
 *
 */
struct bloom
{
  // These fields are part of the public interface of this structure.
  // Client code may read these values if desired. Client code MUST NOT
  // modify any of these.
  size_t entries;
  double error;
  size_t bits;
  size_t bytes;
  size_t qwords;
  int hashes;

  // Fields below are private to the implementation. These may go away or
  // change incompatibly at any moment. Client code MUST NOT access or rely
  // on these.
  double bpe;
  unsigned char *bf;
  uint64_t *bf64;
  int ready;
};

/****
 *
 * function prototypes
 *
 ****/

int bloom_init(struct bloom * bloom, size_t entries, double error);
int bloom_init_64(struct bloom * bloom, size_t entries, double error);
static int bloom_check(struct bloom * bloom, const void * buffer, int len);
static int bloom_check_add(struct bloom * bloom, const void * buffer, int len, int add);
int bloom_check_add_64(struct bloom * bloom, const void * buffer, int len );
int bloom_check_add_64_optimized(struct bloom * bloom, const void * buffer, int len );
static int bloom_add(struct bloom * bloom, const void * buffer, int len);
void bloom_print(struct bloom * bloom);
void bloom_free(struct bloom * bloom);
int bloom_reset(struct bloom * bloom);

#endif /* BLOOM_FILTER_DOT_H */

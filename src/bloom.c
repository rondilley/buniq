/*****
 *
 * $Id: bloom.c,v 1.1.1.1 2013/03/21 01:17:35 rdilley Exp $
 *
 * Copyright (c) 2013, Ron Dilley
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

PRIVATE char *cvsid = "$Id: bloom.c,v 1.1.1.1 2013/03/21 01:17:35 rdilley Exp $";

/* force selection of good primes */
//size_t hashPrimes[] = { 53,97,193,389,769,1543,3079,6151,12289,24593,49157,98317,196613,393241,786433,1572869,3145739,6291469,12582917,25165843,50331653,100663319,201326611,402653189,805306457,1610612741,0 };

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

#include<limits.h>
#include<stdarg.h>

#include"bloom.h"

#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))

BLOOM *bloom_create(size_t size, size_t nfuncs, ...)
{
  BLOOM *bloom;
  va_list l;
  int n;
  
  if(!(bloom=malloc(sizeof(BLOOM)))) return NULL;
  if(!(bloom->a=calloc((size+CHAR_BIT-1)/CHAR_BIT, sizeof(char)))) {
    free(bloom);
    return NULL;
  }
  if(!(bloom->funcs=(hashfunc_t*)malloc(nfuncs*sizeof(hashfunc_t)))) {
    free(bloom->a);
    free(bloom);
    return NULL;
  }
  
  va_start(l, nfuncs);
  for(n=0; n<nfuncs; ++n) {
    bloom->funcs[n]=va_arg(l, hashfunc_t);
  }
  va_end(l);
  
  bloom->nfuncs=nfuncs;
  bloom->asize=size;
  
  return bloom;
}

int bloom_destroy(BLOOM *bloom)
{
  free(bloom->a);
  free(bloom->funcs);
  free(bloom);
  
  return 0;
}

int bloom_add(BLOOM *bloom, const char *s)
{
  size_t n;
  int new = 0;
  
  for(n=0; n<bloom->nfuncs; ++n) {
    if(!(GETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize))) {
      SETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize);
      new = 1;
    }
  }
  
  return new;
}

int bloom_check(BLOOM *bloom, const char *s)
{
  size_t n;
  
  for(n=0; n<bloom->nfuncs; ++n) {
    if(!(GETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize))) return 0;
  }
  
  return 1;
}

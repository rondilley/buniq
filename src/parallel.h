/*****
 *
 * Description: Parallel Processing Headers
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

#ifndef PARALLEL_DOT_H
#define PARALLEL_DOT_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "../include/sysdep.h"
#include "../include/common.h"
#include <pthread.h>
#include <semaphore.h>
#include "bloom-filter.h"
#include "dablooms.h"

/* Thread pool structure */
typedef struct {
  pthread_t *threads;
  int num_threads;
  int shutdown;
  
  /* Work queue */
  char **work_queue;
  int queue_size;
  int queue_front;
  int queue_rear;
  int queue_count;
  
  /* Synchronization */
  pthread_mutex_t queue_mutex;
  pthread_cond_t queue_not_empty;
  pthread_cond_t queue_not_full;
  
  /* Results */
  char **results;
  int *result_counts;
  int result_size;
  int result_count;
  pthread_mutex_t result_mutex;
  
  /* Bloom filter reference */
  void *bloom_filter;
  bloom_type_t bloom_type;
  
} thread_pool_t;

/* Work item structure */
typedef struct {
  char *line;
  int line_len;
  int line_num;
  int is_duplicate;
  int count;
} work_item_t;

/* Function prototypes */
thread_pool_t *create_thread_pool(int num_threads, int queue_size);
void destroy_thread_pool(thread_pool_t *pool);
int submit_work(thread_pool_t *pool, const char *line, int line_len, int line_num);
int get_result(thread_pool_t *pool, work_item_t *item);
void set_bloom_filter(thread_pool_t *pool, void *bloom_filter, bloom_type_t type);
void *worker_thread(void *arg);

/* Parallel processing functions */
int process_file_parallel(const char *filename, int num_threads);
int process_chunk_parallel(FILE *file, off_t start, off_t end, int thread_id);

#endif /* PARALLEL_DOT_H */
/*****
 *
 * Description: Parallel Processing Implementation
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

#include "parallel.h"
#include "main.h"

extern Config_t *config;

/****
 *
 * Create and initialize a thread pool for parallel processing
 *
 * Creates a thread pool with the specified number of worker threads and
 * work queue size. Initializes synchronization primitives, work queue,
 * result storage, and spawns worker threads.
 *
 * Arguments:
 *   num_threads - Number of worker threads to create
 *   queue_size - Maximum size of the work queue
 *
 * Returns:
 *   Pointer to initialized thread_pool_t structure on success, NULL on error
 *
 ****/
thread_pool_t *create_thread_pool(int num_threads, int queue_size) {
  thread_pool_t *pool = (thread_pool_t *)XMALLOC(sizeof(thread_pool_t));
  if (pool == NULL) return NULL;
  
  pool->num_threads = num_threads;
  pool->queue_size = queue_size;
  pool->shutdown = 0;
  
  /* Initialize work queue */
  pool->work_queue = (char **)XMALLOC(queue_size * sizeof(char *));
  pool->queue_front = 0;
  pool->queue_rear = 0;
  pool->queue_count = 0;
  
  /* Initialize results */
  pool->results = (char **)XMALLOC(queue_size * sizeof(char *));
  pool->result_counts = (int *)XMALLOC(queue_size * sizeof(int));
  pool->result_size = queue_size;
  pool->result_count = 0;
  
  /* Initialize synchronization */
  pthread_mutex_init(&pool->queue_mutex, NULL);
  pthread_cond_init(&pool->queue_not_empty, NULL);
  pthread_cond_init(&pool->queue_not_full, NULL);
  pthread_mutex_init(&pool->result_mutex, NULL);
  
  /* Create threads */
  pool->threads = (pthread_t *)XMALLOC(num_threads * sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
      destroy_thread_pool(pool);
      return NULL;
    }
  }
  
  return pool;
}

/****
 *
 * Destroy a thread pool and clean up all resources
 *
 * Signals shutdown to all worker threads, waits for them to complete,
 * destroys synchronization primitives, and frees all allocated memory.
 *
 * Arguments:
 *   pool - Pointer to thread pool structure to destroy
 *
 * Returns:
 *   None
 *
 ****/
void destroy_thread_pool(thread_pool_t *pool) {
  if (pool == NULL) return;
  
  /* Signal shutdown */
  pthread_mutex_lock(&pool->queue_mutex);
  pool->shutdown = 1;
  pthread_cond_broadcast(&pool->queue_not_empty);
  pthread_mutex_unlock(&pool->queue_mutex);
  
  /* Wait for threads to finish */
  for (int i = 0; i < pool->num_threads; i++) {
    pthread_join(pool->threads[i], NULL);
  }
  
  /* Cleanup */
  pthread_mutex_destroy(&pool->queue_mutex);
  pthread_cond_destroy(&pool->queue_not_empty);
  pthread_cond_destroy(&pool->queue_not_full);
  pthread_mutex_destroy(&pool->result_mutex);
  
  XFREE(pool->threads);
  XFREE(pool->work_queue);
  XFREE(pool->results);
  XFREE(pool->result_counts);
  XFREE(pool);
}

/****
 *
 * Submit a work item to the thread pool queue
 *
 * Adds a line of text to the work queue for processing by worker threads.
 * Blocks if the queue is full until space becomes available.
 *
 * Arguments:
 *   pool - Pointer to thread pool structure
 *   line - Text line to be processed
 *   line_len - Length of the line (currently unused)
 *   line_num - Line number for tracking (currently unused)
 *
 * Returns:
 *   0 on success, -1 if pool is shutting down
 *
 ****/
int submit_work(thread_pool_t *pool, const char *line, int line_len, int line_num) {
  pthread_mutex_lock(&pool->queue_mutex);
  
  /* Wait for space in queue */
  while (pool->queue_count == pool->queue_size && !pool->shutdown) {
    pthread_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
  }
  
  if (pool->shutdown) {
    pthread_mutex_unlock(&pool->queue_mutex);
    return -1;
  }
  
  /* Add work to queue */
  pool->work_queue[pool->queue_rear] = strdup(line);
  pool->queue_rear = (pool->queue_rear + 1) % pool->queue_size;
  pool->queue_count++;
  
  pthread_cond_signal(&pool->queue_not_empty);
  pthread_mutex_unlock(&pool->queue_mutex);
  
  return 0;
}

/****
 *
 * Set the bloom filter for duplicate detection in the thread pool
 *
 * Associates a bloom filter with the thread pool for use in duplicate
 * detection during parallel processing.
 *
 * Arguments:
 *   pool - Pointer to thread pool structure
 *   bloom_filter - Pointer to bloom filter structure (regular or scaling)
 *   type - Type of bloom filter (BLOOM_REGULAR or BLOOM_SCALING)
 *
 * Returns:
 *   None
 *
 ****/
void set_bloom_filter(thread_pool_t *pool, void *bloom_filter, bloom_type_t type) {
  pool->bloom_filter = bloom_filter;
  pool->bloom_type = type;
}

/****
 *
 * Worker thread function for processing work items
 *
 * Main loop for worker threads that processes lines from the work queue.
 * Checks for duplicates using the configured bloom filter and stores
 * results for output.
 *
 * Arguments:
 *   arg - Pointer to thread pool structure cast as void*
 *
 * Returns:
 *   NULL when thread exits
 *
 ****/
void *worker_thread(void *arg) {
  thread_pool_t *pool = (thread_pool_t *)arg;
  
  while (1) {
    pthread_mutex_lock(&pool->queue_mutex);
    
    /* Wait for work */
    while (pool->queue_count == 0 && !pool->shutdown) {
      pthread_cond_wait(&pool->queue_not_empty, &pool->queue_mutex);
    }
    
    if (pool->shutdown && pool->queue_count == 0) {
      pthread_mutex_unlock(&pool->queue_mutex);
      break;
    }
    
    /* Get work item */
    char *line = pool->work_queue[pool->queue_front];
    pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
    pool->queue_count--;
    
    pthread_cond_signal(&pool->queue_not_full);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    /* Process the line */
    int is_duplicate = 0;
    int count = 0;
    size_t line_len = strlen(line);
    
    switch (pool->bloom_type) {
      case BLOOM_REGULAR: {
        struct bloom *bf = (struct bloom *)pool->bloom_filter;
        is_duplicate = bloom_check_add_64(bf, line, line_len);
        count = is_duplicate ? 0 : 1;
        break;
      }
      case BLOOM_SCALING: {
        scaling_bloom_t *sbf = (scaling_bloom_t *)pool->bloom_filter;
        static uint64_t line_id = 0;
        is_duplicate = scaling_bloom_check_add(sbf, line, line_len, ++line_id);
        count = is_duplicate ? 0 : 1;
        break;
      }
    }
    
    /* Store result */
    pthread_mutex_lock(&pool->result_mutex);
    if (pool->result_count < pool->result_size) {
      if (!is_duplicate || config->show_duplicates) {
        pool->results[pool->result_count] = strdup(line);
        pool->result_counts[pool->result_count] = count;
        pool->result_count++;
      }
    }
    pthread_mutex_unlock(&pool->result_mutex);
    
    free(line);
  }
  
  return NULL;
}

/****
 *
 * Process a file in parallel using multiple threads
 *
 * Opens the specified file (or stdin if filename is "-"), creates a
 * thread pool with the specified number of threads, initializes the
 * appropriate bloom filter, and processes all lines in parallel.
 *
 * Arguments:
 *   filename - Name of file to process, or "-" for stdin
 *   num_threads - Number of worker threads to use
 *
 * Returns:
 *   TRUE on success, FAILED on error
 *
 ****/
int process_file_parallel(const char *filename, int num_threads) {
  FILE *file;
  char line[8192];
  thread_pool_t *pool;
  
  /* Open file */
  if (strcmp(filename, "-") == 0) {
    file = stdin;
  } else {
    file = fopen(filename, "r");
    if (file == NULL) {
      fprintf(stderr, "ERR - Unable to open file for reading\n");
      return FAILED;
    }
  }
  
  /* Create thread pool */
  pool = create_thread_pool(num_threads, 1000);
  if (pool == NULL) {
    if (file != stdin) fclose(file);
    return FAILED;
  }
  
  /* Set up bloom filter based on config */
  struct bloom bf;
  scaling_bloom_t *sbf = NULL;
  
  if (config->bloom_type == BLOOM_REGULAR) {
    if (bloom_init_64(&bf, 100000, config->eRate) != 0) {
      destroy_thread_pool(pool);
      if (file != stdin) fclose(file);
      return FAILED;
    }
    set_bloom_filter(pool, &bf, BLOOM_REGULAR);
  } else if (config->bloom_type == BLOOM_SCALING) {
    char tmpfile[] = "/tmp/buniq-XXXXXX";
    int tmpfd = mkstemp(tmpfile);
    if (tmpfd == -1) {
      destroy_thread_pool(pool);
      if (file != stdin) fclose(file);
      return FAILED;
    }
    close(tmpfd);
    
    sbf = new_scaling_bloom(1000000, config->eRate, tmpfile);
    if (sbf == NULL) {
      destroy_thread_pool(pool);
      if (file != stdin) fclose(file);
      return FAILED;
    }
    set_bloom_filter(pool, sbf, BLOOM_SCALING);
  }
  
  /* Process lines */
  int line_num = 0;
  while (fgets(line, sizeof(line), file) != NULL) {
    submit_work(pool, line, strlen(line), line_num++);
    config->total_lines++;
  }
  
  /* Wait for processing to complete */
  pthread_mutex_lock(&pool->queue_mutex);
  while (pool->queue_count > 0) {
    pthread_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
  }
  pthread_mutex_unlock(&pool->queue_mutex);
  
  /* Output results */
  pthread_mutex_lock(&pool->result_mutex);
  for (int i = 0; i < pool->result_count; i++) {
    printf("%s", pool->results[i]);
    free(pool->results[i]);
  }
  config->unique_lines = pool->result_count;
  pthread_mutex_unlock(&pool->result_mutex);
  
  /* Cleanup */
  if (config->bloom_type == BLOOM_REGULAR) {
    bloom_free(&bf);
  } else if (config->bloom_type == BLOOM_SCALING) {
    free_scaling_bloom(sbf);
  }
  
  destroy_thread_pool(pool);
  if (file != stdin) fclose(file);
  
  return TRUE;
}
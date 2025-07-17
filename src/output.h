/*****
 *
 * Description: Output Formatting Headers
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

#ifndef OUTPUT_DOT_H
#define OUTPUT_DOT_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "../include/sysdep.h"
#include "../include/common.h"
#include <time.h>
#include <sys/time.h>

/* Progress bar structure */
typedef struct {
  uint64_t total;
  uint64_t current;
  time_t start_time;
  int width;
  int last_percent;
} progress_bar_t;

/* Statistics structure */
typedef struct {
  uint64_t total_lines;
  uint64_t unique_lines;
  uint64_t duplicate_lines;
  double processing_time;
  size_t memory_used;
  double throughput;
  double false_positive_rate;
} stats_t;

/* Function prototypes */
void output_line(const char *line, int count, output_format_t format);
void output_header(output_format_t format);
void output_footer(output_format_t format);
void output_stats(const stats_t *stats, output_format_t format);

/* Progress bar functions */
progress_bar_t *create_progress_bar(uint64_t total, int width);
void update_progress_bar(progress_bar_t *bar, uint64_t current);
void finish_progress_bar(progress_bar_t *bar);
void destroy_progress_bar(progress_bar_t *bar);

/* Statistics functions */
void init_stats(stats_t *stats);
void update_stats(stats_t *stats, int is_unique);
void finalize_stats(stats_t *stats, double processing_time, size_t memory_used);

/* JSON output functions */
void output_json_start(void);
void output_json_line(const char *line, int count, int is_last);
void output_json_end(const stats_t *stats);

/* CSV output functions */
void output_csv_header(void);
void output_csv_line(const char *line, int count, int line_num);

/* Utility functions */
char *escape_json_string(const char *str);
char *escape_csv_string(const char *str);
double get_time_diff(struct timeval *start, struct timeval *end);

#endif /* OUTPUT_DOT_H */
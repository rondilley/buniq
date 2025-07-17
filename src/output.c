/*****
 *
 * Description: Output Formatting Implementation
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

#include "output.h"
#include "main.h"

extern Config_t *config;

/****
 *
 * Outputs a line of text in the specified format
 *
 * Formats and outputs a single line of text according to the specified
 * output format (text, JSON, CSV, or TSV). Maintains internal line numbering
 * for formats that require it.
 *
 * Arguments:
 *   line - The text line to output
 *   count - Count of occurrences (unused in current implementation)
 *   format - The output format to use (OUTPUT_TEXT, OUTPUT_JSON, etc.)
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_line(const char *line, int count, output_format_t format) {
  static int line_num = 0;
  line_num++;
  
  switch (format) {
    case OUTPUT_TEXT:
      printf("%s", line);
      break;
      
    case OUTPUT_JSON:
      output_json_line(line, count, 0);
      break;
      
    case OUTPUT_CSV:
      output_csv_line(line, count, line_num);
      break;
      
    case OUTPUT_TSV:
      printf("%s", line);
      break;
  }
}

/****
 *
 * Outputs format-specific header information
 *
 * Outputs any necessary header information for the specified output format.
 * JSON format outputs opening JSON structure, CSV outputs column headers,
 * and TEXT/TSV formats require no header.
 *
 * Arguments:
 *   format - The output format to use (OUTPUT_TEXT, OUTPUT_JSON, etc.)
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_header(output_format_t format) {
  switch (format) {
    case OUTPUT_JSON:
      output_json_start();
      break;
      
    case OUTPUT_CSV:
      output_csv_header();
      break;
      
    case OUTPUT_TEXT:
    case OUTPUT_TSV:
    default:
      /* No header needed */
      break;
  }
}

/****
 *
 * Outputs format-specific footer information
 *
 * Outputs any necessary footer information for the specified output format.
 * Currently only JSON format may require footer handling, though it is
 * handled in output_json_end function.
 *
 * Arguments:
 *   format - The output format to use (OUTPUT_TEXT, OUTPUT_JSON, etc.)
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_footer(output_format_t format) {
  switch (format) {
    case OUTPUT_JSON:
      /* Will be handled in output_json_end */
      break;
      
    case OUTPUT_TEXT:
    case OUTPUT_CSV:
    case OUTPUT_TSV:
    default:
      /* No footer needed */
      break;
  }
}

/****
 *
 * Outputs statistics information in the specified format
 *
 * Outputs processing statistics including total lines, unique lines,
 * duplicate lines, processing time, memory usage, and throughput.
 * JSON format includes stats in the output structure, while other
 * formats output to stderr.
 *
 * Arguments:
 *   stats - Pointer to statistics structure containing processing metrics
 *   format - The output format to use (OUTPUT_TEXT, OUTPUT_JSON, etc.)
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_stats(const stats_t *stats, output_format_t format) {
  switch (format) {
    case OUTPUT_JSON:
      output_json_end(stats);
      break;
      
    case OUTPUT_TEXT:
    case OUTPUT_CSV:
    case OUTPUT_TSV:
    default:
      fprintf(stderr, "\nStatistics:\n");
      fprintf(stderr, "  Total lines: %lu\n", stats->total_lines);
      fprintf(stderr, "  Unique lines: %lu\n", stats->unique_lines);
      fprintf(stderr, "  Duplicate lines: %lu\n", stats->duplicate_lines);
      fprintf(stderr, "  Processing time: %.3f seconds\n", stats->processing_time);
      fprintf(stderr, "  Memory used: %lu bytes\n", stats->memory_used);
      fprintf(stderr, "  Throughput: %.0f lines/second\n", stats->throughput);
      if (stats->false_positive_rate > 0) {
        fprintf(stderr, "  False positive rate: %.4f%%\n", stats->false_positive_rate * 100);
      }
      break;
  }
}

/****
 *
 * Creates and initializes a progress bar structure
 *
 * Allocates memory for a progress bar structure and initializes it with
 * the specified total count and display width. Sets up timing information
 * and initial state for progress tracking.
 *
 * Arguments:
 *   total - Total number of items to process
 *   width - Display width of the progress bar in characters
 *
 * Returns:
 *   Pointer to newly created progress_bar_t structure, or NULL on error
 *
 ****/
progress_bar_t *create_progress_bar(uint64_t total, int width) {
  progress_bar_t *bar = (progress_bar_t *)XMALLOC(sizeof(progress_bar_t));
  if (bar == NULL) return NULL;
  
  bar->total = total;
  bar->current = 0;
  bar->start_time = time(NULL);
  bar->width = width;
  bar->last_percent = -1;
  
  return bar;
}

/****
 *
 * Updates progress bar display with current progress
 *
 * Updates the progress bar display with the current progress count.
 * Calculates percentage complete, estimated time remaining, and displays
 * a visual progress bar. Only updates display when percentage changes
 * to reduce flicker.
 *
 * Arguments:
 *   bar - Pointer to progress bar structure to update
 *   current - Current progress count
 *
 * Returns:
 *   None (void function)
 *
 ****/
void update_progress_bar(progress_bar_t *bar, uint64_t current) {
  if (bar == NULL) return;
  
  bar->current = current;
  
  if (bar->total == 0) return;
  
  int percent = (int)((current * 100) / bar->total);
  
  if (percent == bar->last_percent) return;
  
  bar->last_percent = percent;
  
  time_t now = time(NULL);
  time_t elapsed = now - bar->start_time;
  time_t eta = (elapsed * bar->total) / current - elapsed;
  
  int filled = (percent * bar->width) / 100;
  
  fprintf(stderr, "\r[");
  for (int i = 0; i < bar->width; i++) {
    if (i < filled) {
      fprintf(stderr, "=");
    } else if (i == filled) {
      fprintf(stderr, ">");
    } else {
      fprintf(stderr, " ");
    }
  }
  fprintf(stderr, "] %d%% (%lu/%lu) ETA: %ldm%lds", 
          percent, current, bar->total, eta / 60, eta % 60);
  fflush(stderr);
}

/****
 *
 * Completes and finalizes progress bar display
 *
 * Displays a completed progress bar (100%) with total elapsed time.
 * Shows final completion message with timing information.
 *
 * Arguments:
 *   bar - Pointer to progress bar structure to finalize
 *
 * Returns:
 *   None (void function)
 *
 ****/
void finish_progress_bar(progress_bar_t *bar) {
  if (bar == NULL) return;
  
  time_t elapsed = time(NULL) - bar->start_time;
  fprintf(stderr, "\r[");
  for (int i = 0; i < bar->width; i++) {
    fprintf(stderr, "=");
  }
  fprintf(stderr, "] 100%% (%lu/%lu) Completed in %ldm%lds\n",
          bar->current, bar->total, elapsed / 60, elapsed % 60);
}

/****
 *
 * Destroys and deallocates progress bar structure
 *
 * Frees the memory allocated for the progress bar structure.
 * Safe to call with NULL pointer.
 *
 * Arguments:
 *   bar - Pointer to progress bar structure to destroy
 *
 * Returns:
 *   None (void function)
 *
 ****/
void destroy_progress_bar(progress_bar_t *bar) {
  if (bar != NULL) {
    XFREE(bar);
  }
}

/****
 *
 * Initializes statistics structure with default values
 *
 * Sets all fields in the statistics structure to zero or default values.
 * Must be called before using the statistics structure.
 *
 * Arguments:
 *   stats - Pointer to statistics structure to initialize
 *
 * Returns:
 *   None (void function)
 *
 ****/
void init_stats(stats_t *stats) {
  stats->total_lines = 0;
  stats->unique_lines = 0;
  stats->duplicate_lines = 0;
  stats->processing_time = 0.0;
  stats->memory_used = 0;
  stats->throughput = 0.0;
  stats->false_positive_rate = 0.0;
}

/****
 *
 * Updates statistics counters for processed line
 *
 * Increments the total line counter and either unique or duplicate
 * line counter based on whether the line was unique.
 *
 * Arguments:
 *   stats - Pointer to statistics structure to update
 *   is_unique - Non-zero if line was unique, zero if duplicate
 *
 * Returns:
 *   None (void function)
 *
 ****/
void update_stats(stats_t *stats, int is_unique) {
  stats->total_lines++;
  if (is_unique) {
    stats->unique_lines++;
  } else {
    stats->duplicate_lines++;
  }
}

/****
 *
 * Finalizes statistics with timing and memory information
 *
 * Completes the statistics structure by adding processing time and
 * memory usage information. Calculates throughput and false positive
 * rate based on the collected data.
 *
 * Arguments:
 *   stats - Pointer to statistics structure to finalize
 *   processing_time - Total processing time in seconds
 *   memory_used - Total memory used in bytes
 *
 * Returns:
 *   None (void function)
 *
 ****/
void finalize_stats(stats_t *stats, double processing_time, size_t memory_used) {
  stats->processing_time = processing_time;
  stats->memory_used = memory_used;
  
  if (processing_time > 0) {
    stats->throughput = stats->total_lines / processing_time;
  }
  
  /* Calculate false positive rate (approximate) */
  if (stats->total_lines > 0) {
    stats->false_positive_rate = config->eRate;
  }
}

/****
 *
 * Outputs JSON format opening structure
 *
 * Outputs the opening JSON structure including format identifier,
 * version information, and begins the lines array.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_json_start(void) {
  printf("{\n");
  printf("  \"format\": \"buniq-json\",\n");
  printf("  \"version\": \"1.0\",\n");
  printf("  \"lines\": [\n");
}

/****
 *
 * Outputs a single line in JSON format
 *
 * Outputs a line as a JSON object within the lines array. Handles
 * proper JSON formatting including commas between objects and
 * escaping of special characters in the line content.
 *
 * Arguments:
 *   line - The text line to output
 *   count - Count of occurrences (unused in current implementation)
 *   is_last - Non-zero if this is the last line (unused in current implementation)
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_json_line(const char *line, int count, int is_last) {
  static int first_line = 1;
  
  if (!first_line) {
    printf(",\n");
  }
  first_line = 0;
  
  char *escaped = escape_json_string(line);
  printf("    {");
  printf("\"line\": \"%s\"", escaped);
  
  
  printf("}");
  
  free(escaped);
}

/****
 *
 * Completes JSON output with statistics and closing structure
 *
 * Closes the lines array and adds a statistics object containing
 * processing metrics. Completes the JSON output with proper closing
 * braces.
 *
 * Arguments:
 *   stats - Pointer to statistics structure containing processing metrics
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_json_end(const stats_t *stats) {
  printf("\n  ],\n");
  printf("  \"statistics\": {\n");
  printf("    \"total_lines\": %lu,\n", stats->total_lines);
  printf("    \"unique_lines\": %lu,\n", stats->unique_lines);
  printf("    \"duplicate_lines\": %lu,\n", stats->duplicate_lines);
  printf("    \"processing_time\": %.3f,\n", stats->processing_time);
  printf("    \"memory_used\": %lu,\n", stats->memory_used);
  printf("    \"throughput\": %.0f,\n", stats->throughput);
  printf("    \"false_positive_rate\": %.6f\n", stats->false_positive_rate);
  printf("  }\n");
  printf("}\n");
}

/****
 *
 * Outputs CSV format column headers
 *
 * Outputs the column header row for CSV format output.
 * Currently outputs a single "line" column header.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_csv_header(void) {
  printf("line\n");
}

/****
 *
 * Outputs a single line in CSV format
 *
 * Outputs a line as a CSV record with proper escaping of special
 * characters. The line is quoted and any embedded quotes are escaped.
 *
 * Arguments:
 *   line - The text line to output
 *   count - Count of occurrences (unused in current implementation)
 *   line_num - Line number (unused in current implementation)
 *
 * Returns:
 *   None (void function)
 *
 ****/
void output_csv_line(const char *line, int count, int line_num) {
  char *escaped = escape_csv_string(line);
  
  printf("\"%s\"\n", escaped);
  
  free(escaped);
}

/****
 *
 * Escapes special characters for JSON string output
 *
 * Creates a new string with JSON-safe escaping of special characters
 * including quotes, backslashes, and control characters. The caller
 * is responsible for freeing the returned string.
 *
 * Arguments:
 *   str - The string to escape
 *
 * Returns:
 *   Pointer to newly allocated escaped string, or NULL on error
 *
 ****/
char *escape_json_string(const char *str) {
  size_t len = strlen(str);
  char *escaped = (char *)XMALLOC(len * 2 + 1);
  
  int j = 0;
  for (int i = 0; i < len; i++) {
    switch (str[i]) {
      case '"':
        escaped[j++] = '\\';
        escaped[j++] = '"';
        break;
      case '\\':
        escaped[j++] = '\\';
        escaped[j++] = '\\';
        break;
      case '\n':
        escaped[j++] = '\\';
        escaped[j++] = 'n';
        break;
      case '\r':
        escaped[j++] = '\\';
        escaped[j++] = 'r';
        break;
      case '\t':
        escaped[j++] = '\\';
        escaped[j++] = 't';
        break;
      default:
        escaped[j++] = str[i];
        break;
    }
  }
  escaped[j] = '\0';
  
  return escaped;
}

/****
 *
 * Escapes special characters for CSV string output
 *
 * Creates a new string with CSV-safe escaping of special characters.
 * Doubles embedded quotes and converts newlines to spaces. The caller
 * is responsible for freeing the returned string.
 *
 * Arguments:
 *   str - The string to escape
 *
 * Returns:
 *   Pointer to newly allocated escaped string, or NULL on error
 *
 ****/
char *escape_csv_string(const char *str) {
  size_t len = strlen(str);
  char *escaped = (char *)XMALLOC(len * 2 + 1);
  
  int j = 0;
  for (int i = 0; i < len; i++) {
    if (str[i] == '"') {
      escaped[j++] = '"';
      escaped[j++] = '"';
    } else if (str[i] == '\n') {
      /* Remove newlines in CSV */
      escaped[j++] = ' ';
    } else {
      escaped[j++] = str[i];
    }
  }
  escaped[j] = '\0';
  
  return escaped;
}

/****
 *
 * Calculates time difference between two timeval structures
 *
 * Computes the time difference in seconds between two timeval structures,
 * accounting for both seconds and microseconds components.
 *
 * Arguments:
 *   start - Pointer to starting time structure
 *   end - Pointer to ending time structure
 *
 * Returns:
 *   Time difference in seconds as a double
 *
 ****/
double get_time_diff(struct timeval *start, struct timeval *end) {
  return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1000000.0;
}
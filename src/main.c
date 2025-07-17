/*****
 *
 * Description: Main functions
 * 
 * Copyright (c) 2008-2025, Ron Dilley
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
 * includes
 *
 ****/

#include "main.h"
#include <sys/time.h>

/****
 *
 * local variables
 *
 ****/

/****
 *
 * global variables
 *
 ****/

PUBLIC int quit = FALSE;
PUBLIC int reload = FALSE;
PUBLIC Config_t *config = NULL;
PUBLIC int diffIt = FALSE;
PUBLIC int baseDirLen;
PUBLIC int compDirLen;
PUBLIC char baseDir[PATH_MAX];
PUBLIC char compDir[PATH_MAX];

/* hashes */
struct hash_s *lineHash = NULL;

/****
 *
 * external variables
 *
 ****/

extern int errno;
extern char **environ;

/****
 *
 * Main entry point for the buniq program
 *
 * Initializes the program configuration, parses command line arguments,
 * sets up security audit system, and processes input files to remove
 * duplicate lines using bloom filters.
 *
 * Arguments:
 *   argc - Number of command line arguments
 *   argv - Array of command line argument strings
 *
 * Returns:
 *   EXIT_SUCCESS on successful completion
 *   EXIT_FAILURE on error
 *
 ****/

int main(int argc, char *argv[]) {
  PRIVATE int pid = 0;
  PRIVATE int c = 0, i = 0, fds = 0, status = 0;
  int digit_optind = 0;
  PRIVATE struct passwd *pwd_ent;
  PRIVATE struct group *grp_ent;
  PRIVATE char **ptr;
  char *tmp_ptr = NULL;
  char *pid_file = NULL;
  char *home_dir = NULL;
  char *chroot_dir = NULL;
  char *user = NULL;
  char *group = NULL;

  /* Initialize security audit system */
  security_audit_init();
  
  /* setup config */
  config = ( Config_t * )XMALLOC( sizeof( Config_t ) );
  XMEMSET( config, 0, sizeof( Config_t ) );
  
  /* Initialize new features with defaults */
  config->num_threads = 1;
  config->show_stats = FALSE;
  config->show_progress = FALSE;
  config->show_duplicates = FALSE;
  config->count_duplicates = FALSE;
  config->output_format = OUTPUT_TEXT;
  config->bloom_type = BLOOM_REGULAR;
  config->save_bloom_file = NULL;
  config->load_bloom_file = NULL;
  config->adaptive_sizing = FALSE;

  /* store current pid */
  config->cur_pid = getpid();

  /* store current user record */
  config->starting_uid = getuid();
  pwd_ent = getpwuid( config->starting_uid );
  if ( pwd_ent EQ NULL ) {
    fprintf( stderr, "Unable to get user's record\n" );
    endpwent();
    exit( EXIT_FAILURE );
  }
  if ( ( tmp_ptr = strdup( pwd_ent->pw_dir ) ) EQ NULL ) {
    fprintf( stderr, "Unable to dup home dir\n" );
    endpwent();
    exit( EXIT_FAILURE );
  }
  /* set home dir */
  home_dir = ( char * )XMALLOC( MAXPATHLEN + 1 );
  strncpy( home_dir, pwd_ent->pw_dir, MAXPATHLEN );
  endpwent();

  /* get real uid and gid in prep for priv drop */
  config->gid = getgid();
  config->uid = getuid();

  while (1) {
    int this_option_optind = optind ? optind : 1;
#ifdef HAVE_GETOPT_LONG
    int option_index = 0;
    static struct option long_options[] = {
      {"version", no_argument, 0, 'v' },
      {"debug", required_argument, 0, 'd' },
      {"error", required_argument, 0, 'e' },
      {"help", no_argument, 0, 'h' },
      {"threads", required_argument, 0, 'j' },
      {"count", no_argument, 0, 'c' },
      {"stats", no_argument, 0, 's' },
      {"progress", no_argument, 0, 'p' },
      {"duplicates", no_argument, 0, 'D' },
      {"format", required_argument, 0, 'f' },
      {"bloom-type", required_argument, 0, 'b' },
      {"save-bloom", required_argument, 0, 'S' },
      {"load-bloom", required_argument, 0, 'L' },
      {"adaptive", no_argument, 0, 'a' },
      {0, no_argument, 0, 0}
    };
    c = getopt_long(argc, argv, "vd:e:hj:cspDf:b:S:L:a", long_options, &option_index);
#else
    c = getopt( argc, argv, "vd:e:h" );
#endif

    if (c EQ -1)
      break;

    switch (c) {

    case 'v':
      /* show the version */
      print_version();
      return( EXIT_SUCCESS );

    case 'd':
      /* show debig info */
      config->debug = atoi( optarg );
      config->mode = MODE_INTERACTIVE;
      break;

    case 'e':
      /* override default bf error rate */
      config->eRate = atof( optarg );
      if ( config->eRate <= 0.0 || config->eRate >= 1.0 ) {
        fprintf( stderr, "ERR - Error rate must be between 0.0 and 1.0\n" );
        return( EXIT_FAILURE );
      }
      break;

    case 'h':
      /* show help info */
      print_help();
      return( EXIT_SUCCESS );

    case 'j':
      /* number of threads */
      config->num_threads = atoi( optarg );
      if ( config->num_threads < 1 || config->num_threads > 64 ) {
        fprintf( stderr, "ERR - Number of threads must be between 1 and 64\n" );
        return( EXIT_FAILURE );
      }
      break;

    case 'c':
      /* count duplicates */
      config->count_duplicates = TRUE;
      break;

    case 's':
      /* show statistics */
      config->show_stats = TRUE;
      break;

    case 'p':
      /* show progress */
      config->show_progress = TRUE;
      break;

    case 'D':
      /* show duplicates instead of unique */
      config->show_duplicates = TRUE;
      break;

    case 'f':
      /* output format */
      if ( strcmp( optarg, "text" ) == 0 ) {
        config->output_format = OUTPUT_TEXT;
      } else if ( strcmp( optarg, "json" ) == 0 ) {
        config->output_format = OUTPUT_JSON;
      } else if ( strcmp( optarg, "csv" ) == 0 ) {
        config->output_format = OUTPUT_CSV;
      } else if ( strcmp( optarg, "tsv" ) == 0 ) {
        config->output_format = OUTPUT_TSV;
      } else {
        fprintf( stderr, "ERR - Invalid output format: %s\n", optarg );
        return( EXIT_FAILURE );
      }
      break;

    case 'b':
      /* bloom filter type */
      if ( strcmp( optarg, "regular" ) == 0 ) {
        config->bloom_type = BLOOM_REGULAR;
      } else if ( strcmp( optarg, "scaling" ) == 0 ) {
        config->bloom_type = BLOOM_SCALING;
      } else {
        fprintf( stderr, "ERR - Invalid bloom filter type: %s (use regular or scaling)\n", optarg );
        return( EXIT_FAILURE );
      }
      break;

    case 'S':
      /* save bloom filter */
      config->save_bloom_file = strdup( optarg );
      break;

    case 'L':
      /* load bloom filter */
      config->load_bloom_file = strdup( optarg );
      break;

    case 'a':
      /* adaptive sizing */
      config->adaptive_sizing = TRUE;
      break;

    default:
      fprintf( stderr, "Unknown option code [0%o]\n", c);
    }
  }

  /* set default error rate if not set */
  if ( config->eRate EQ 0 )
    config->eRate = 0.01;

  /* check dirs and files for danger */

  if ( time( &config->current_time ) EQ -1 ) {
    display( LOG_ERR, "Unable to get current time" );
    /* cleanup buffers */
    cleanup();
    return EXIT_FAILURE;
  }

  /* initialize program wide config options */
  config->hostname = (char *)XMALLOC( MAXHOSTNAMELEN+1 );

  /* get processor hostname */
  if ( gethostname( config->hostname, MAXHOSTNAMELEN ) != 0 ) {
    display( LOG_ERR, "Unable to get hostname" );
    strcpy( config->hostname, "unknown" );
  }

  /****
   *
   * lets get this party started
   *
   ****/

  /* Only show info in debug mode */
  if ( config->debug > 0 ) {
    show_info();
  }

  /* Initialize timing */
  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);
  
  if (optind < argc) {
    /* Process specified file */
    if (config->num_threads > 1) {
      process_file_parallel( argv[optind++], config->num_threads );
    } else {
      processFile( argv[optind++] );
    }
  } else {
    /* No file specified, read from stdin */
    if (config->num_threads > 1) {
      process_file_parallel( "-", config->num_threads );
    } else {
      processFile( "-" );
    }
  }
  
  /* Calculate processing time */
  gettimeofday(&end_time, NULL);
  config->processing_time = get_time_diff(&start_time, &end_time);
  
  /* Show statistics if requested */
  if (config->show_stats) {
    stats_t stats;
    init_stats(&stats);
    stats.total_lines = config->total_lines;
    stats.unique_lines = config->unique_lines;
    stats.duplicate_lines = config->duplicate_lines;
    finalize_stats(&stats, config->processing_time, config->memory_used);
    output_stats(&stats, config->output_format);
  }

  /****
   *
   * we are done
   *
   ****/

  /* Security cleanup */
  secure_cleanup_temp_files();
  security_audit_cleanup();

  cleanup();

  return( EXIT_SUCCESS );
}

/****
 *
 * Display program information including version and license details
 *
 * Outputs program name, version, build date/time, author information,
 * and GNU General Public License warranty disclaimer to stderr.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void)
 *
 ****/

void show_info( void ) {
  fprintf( stderr, "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
  fprintf( stderr, "By: Ron Dilley\n" );
  fprintf( stderr, "\n" );
  fprintf( stderr, "%s comes with ABSOLUTELY NO WARRANTY.\n", PROGNAME );
  fprintf( stderr, "This is free software, and you are welcome\n" );
  fprintf( stderr, "to redistribute it under certain conditions;\n" );
  fprintf( stderr, "See the GNU General Public License for details.\n" );
  fprintf( stderr, "\n" );
}

/****
 *
 * Display version information
 *
 * Prints the program name, version number, and build date/time
 * to stdout in a concise format.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void)
 *
 ****/

PRIVATE void print_version( void ) {
  printf( "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
}

/****
 *
 * Display comprehensive help information
 *
 * Prints program usage syntax, command line options, and examples
 * to stderr. Shows different option formats depending on whether
 * long options are supported.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void)
 *
 ****/

PRIVATE void print_help( void ) {
  print_version();

  fprintf( stderr, "\n" );
  fprintf( stderr, "syntax: %s [options] [file]\n", PACKAGE );
  fprintf( stderr, "\n" );
  fprintf( stderr, "Reads from stdin if no file is specified.\n" );
  fprintf( stderr, "Uses dynamic bloom filters for large files (>100MB) or stdin.\n" );
  fprintf( stderr, "\n" );

#ifdef HAVE_GETOPT_LONG
  fprintf( stderr, "Basic Options:\n" );
  fprintf( stderr, " -d|--debug (0-9)     enable debugging info\n" );
  fprintf( stderr, " -e|--error (rate)    error rate [default: 0.01]\n" );
  fprintf( stderr, " -h|--help            this info\n" );
  fprintf( stderr, " -v|--version         display version information\n" );
  fprintf( stderr, "\n" );
  fprintf( stderr, "Advanced Options:\n" );
  fprintf( stderr, " -j|--threads (N)     use N threads for parallel processing\n" );
  fprintf( stderr, " -c|--count           count duplicate occurrences\n" );
  fprintf( stderr, " -s|--stats           show processing statistics\n" );
  fprintf( stderr, " -p|--progress        show progress bar\n" );
  fprintf( stderr, " -D|--duplicates      show duplicate lines instead of unique\n" );
  fprintf( stderr, " -f|--format (type)   output format: text, json, csv, tsv\n" );
  fprintf( stderr, " -b|--bloom-type (t)  bloom filter type: regular, scaling\n" );
  fprintf( stderr, " -S|--save-bloom (f)  save bloom filter to file\n" );
  fprintf( stderr, " -L|--load-bloom (f)  load bloom filter from file\n" );
  fprintf( stderr, " -a|--adaptive        use adaptive bloom filter sizing\n" );
#else
  fprintf( stderr, " -d (0-9)   enable debugging info\n" );
  fprintf( stderr, " -e (rate)  error rate [default: 0.01]\n" );
  fprintf( stderr, " -h         this info\n" );
  fprintf( stderr, " -v         display version information\n" );
  fprintf( stderr, " -j (N)     use N threads for parallel processing\n" );
  fprintf( stderr, " -c         count duplicate occurrences\n" );
  fprintf( stderr, " -s         show processing statistics\n" );
  fprintf( stderr, " -p         show progress bar\n" );
  fprintf( stderr, " -D         show duplicate lines instead of unique\n" );
  fprintf( stderr, " -f (type)  output format: text, json, csv, tsv\n" );
  fprintf( stderr, " -b (type)  bloom filter type: regular, scaling\n" );
  fprintf( stderr, " -S (file)  save bloom filter to file\n" );
  fprintf( stderr, " -L (file)  load bloom filter from file\n" );
  fprintf( stderr, " -a         use adaptive bloom filter sizing\n" );
#endif

  fprintf( stderr, "\n" );
  fprintf( stderr, "Examples:\n" );
  fprintf( stderr, "  %s input.txt                    # Remove duplicates from file\n", PACKAGE );
  fprintf( stderr, "  cat file | %s                  # Remove duplicates from stdin\n", PACKAGE );
  fprintf( stderr, "  %s -e 0.001 large.txt          # Use lower error rate for better accuracy\n", PACKAGE );
  fprintf( stderr, "  %s -j 4 -s large.txt           # Use 4 threads and show statistics\n", PACKAGE );
  fprintf( stderr, "  %s -c -f json data.txt         # Count duplicates and output as JSON\n", PACKAGE );
  fprintf( stderr, "  %s -D -p huge.txt              # Show duplicates with progress bar\n", PACKAGE );
  fprintf( stderr, "  %s -b scaling -a input.txt     # Use adaptive scaling bloom filter\n", PACKAGE );
  fprintf( stderr, "\n" );
}

/****
 *
 * Cleanup program resources and memory
 *
 * Frees allocated memory for configuration structures, hostnames,
 * and bloom filter file paths. Performs memory debugging cleanup
 * if enabled.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void)
 *
 ****/

PRIVATE void cleanup( void ) {
  int i, j;

  XFREE( config->hostname );
  
  /* Free new string fields */
  if ( config->save_bloom_file ) {
    free( config->save_bloom_file );
  }
  if ( config->load_bloom_file ) {
    free( config->load_bloom_file );
  }
  
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
  XFREE( config );
}

/****
 *
 * Process input file to remove duplicate lines using bloom filters
 *
 * Processes the specified file (or stdin if "-") to identify and output
 * only unique lines. Uses either regular bloom filters for smaller files
 * or scaling bloom filters for larger files and stdin. Applies security
 * validation to file paths and implements buffered reading for performance.
 *
 * Arguments:
 *   fName - Path to input file, or "-" for stdin
 *
 * Returns:
 *   TRUE on successful processing
 *   FAILED on error (invalid file, security violation, etc.)
 *
 ****/

int processFile( const char *fName ) {
  struct stat fStatBuf;
  size_t fSize = 0;
  FILE *inFile;
  char rBuf[8192];
  char *readBuf = NULL;
  size_t readBufSize = 1024 * 1024; /* 1MB read buffer */
  scaling_bloom_t *sbf = NULL;
  struct bloom bf;
  int use_scaling = FALSE;
  char tmpfile[PATH_MAX];
  uint64_t line_count = 0;

  /* Check if we're reading from stdin or if file is very large */
  if ( strcmp( fName, "-" ) EQ 0 ) {
    use_scaling = TRUE;
    inFile = stdin;
  } else {
    /* Security validation for file path */
    if ( secure_validate_path( fName ) != 0 ) {
      fprintf( stderr, "ERR - Invalid or unsafe file path\n" );
      security_audit_log("FILE_PATH_VALIDATION_FAILED", fName);
      return FAILED;
    }
    
    /* get status on file */
    if ( stat( fName, &fStatBuf ) EQ FAILED ) {
      fprintf( stderr, "ERR - Unable to access input file\n" );
      return FAILED;
    }
    
    /* Basic security check - don't process special files */
    if ( !S_ISREG( fStatBuf.st_mode ) ) {
      fprintf( stderr, "ERR - Input must be a regular file\n" );
      security_audit_log("FILE_TYPE_VALIDATION_FAILED", fName);
      return FAILED;
    }
    
    /* Check file size limits */
    if ( fStatBuf.st_size > (1024 * 1024 * 1024) ) { /* 1GB limit */
      fprintf( stderr, "ERR - File too large (>1GB)\n" );
      security_audit_log("FILE_SIZE_LIMIT_EXCEEDED", fName);
      return FAILED;
    }
    
    fSize = fStatBuf.st_size;
    
    /* Use scaling bloom filter for larger files or stdin */
    /* Lower threshold since scaling bloom filter is more reliable */
    if ( fSize > 10 * 1024 * 1024 ) { /* > 10MB */
      use_scaling = TRUE;
    }
    
#ifdef HAVE_FOPEN64
    if ( ( inFile = fopen64( fName, "r" ) ) EQ NULL ) {
#else
    if ( ( inFile = fopen( fName, "r" ) ) EQ NULL ) {
#endif
      fprintf( stderr, "ERR - Unable to open input file for reading\n" );
      return( EXIT_FAILURE );
    }
  }

  if ( use_scaling ) {
    /* Create secure temporary file for scaling bloom filter */
    char tmpfile_template[] = "/tmp/buniq-XXXXXX";
    int tmpfd = mkstemp( tmpfile_template );
    if ( tmpfd == -1 ) {
      fprintf( stderr, "ERR - Unable to create secure temporary file\n" );
      if ( inFile != stdin ) fclose( inFile );
      return FAILED;
    }
    close( tmpfd ); /* Close fd, let new_scaling_bloom reopen */
    strncpy( tmpfile, tmpfile_template, sizeof(tmpfile) - 1 );
    tmpfile[sizeof(tmpfile) - 1] = '\0';
    
    /* Initialize scaling bloom filter with initial capacity */
    sbf = new_scaling_bloom( 1000000, config->eRate, tmpfile );
    if ( sbf == NULL ) {
      fprintf( stderr, "ERR - Unable to initialize scaling bloom filter\n" );
      if ( inFile != stdin ) fclose( inFile );
      return FAILED;
    }
    
    if ( config->debug > 0 ) {
      fprintf( stderr, "Using scaling bloom filter with error rate %.4f\n", config->eRate );
    }
    
    /* For larger files, use optimized buffered reading */
    if ( fSize > 10 * 1024 * 1024 ) { /* > 10MB */
      /* Allocate large read buffer */
      readBuf = (char *)XMALLOC( readBufSize );
      if ( readBuf == NULL ) {
        fprintf( stderr, "ERR - Unable to allocate read buffer\n" );
        if ( inFile != stdin ) fclose( inFile );
        free_scaling_bloom( sbf );
        unlink( tmpfile );
        return FAILED;
      }
      
      /* Set large buffer for stdio */
      if ( setvbuf( inFile, NULL, _IOFBF, readBufSize ) != 0 ) {
        fprintf( stderr, "WARN - Unable to set large buffer\n" );
      }
    }
    
    /* Process lines with scaling bloom filter */
    while ( fgets( rBuf, sizeof( rBuf ), inFile ) != NULL ) {
      line_count++;
      size_t line_len = strlen( rBuf );
      /* Combined check and add to avoid duplicate hash computation */
      if ( scaling_bloom_check_add( sbf, rBuf, line_len, line_count ) == 0 ) {
        /* This is a new unique line, print it */
        printf( "%s", rBuf );
      }
    }
    
    /* Cleanup */
    if ( readBuf != NULL ) {
      XFREE( readBuf );
    }
    free_scaling_bloom( sbf );
    unlink( tmpfile );
    
  } else {
    /* Use regular bloom filter for smaller files */
    
    /* Estimate lines based on average line length of 20 chars (more realistic) */
    /* Add 50% buffer for safety */
    size_t estimated_lines = ((fSize / 20) * 3) / 2;
    
    /* Ensure reasonable bounds */
    if (estimated_lines < 1000) estimated_lines = 1000;
    if (estimated_lines > 10000000) estimated_lines = 10000000;
    
    /* init bloom filter */
    if ( bloom_init_64( &bf, estimated_lines, config->eRate ) != 0 ) {
      fprintf( stderr, "ERR - Unable to initialize bloom filter\n" );
      if ( inFile != stdin ) fclose( inFile );
      return FAILED;
    }
    
    if ( config->debug > 0 ) {
      bloom_print( &bf );
    }
    
    /* Process lines with regular bloom filter */
    while ( fgets( rBuf, sizeof( rBuf ), inFile ) != NULL ) {
      size_t line_len = strlen( rBuf );
      /* Use original check-and-add function (fixed bit shift) */
      if ( bloom_check_add_64( &bf, rBuf, line_len ) == 0 ) {
        /* This is a new unique line, print it */
        printf( "%s", rBuf );
      }
    }
    
    /* Cleanup regular bloom filter */
    bloom_free( &bf );
  }

  /* close file */
  if ( inFile != stdin ) {
    fclose( inFile );
  }
  
  return TRUE;
}
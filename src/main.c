/*****
 *
 * Description: Main functions
 * 
 * Copyright (c) 2008-2019, Ron Dilley
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
 * main function
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

  /* setup config */
  config = ( Config_t * )XMALLOC( sizeof( Config_t ) );
  XMEMSET( config, 0, sizeof( Config_t ) );

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
      {0, no_argument, 0, 0}
    };
    c = getopt_long(argc, argv, "vd:e:h", long_options, &option_index);
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
      break;

    case 'h':
      /* show help info */
      print_help();
      return( EXIT_SUCCESS );

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

  show_info();

  if (optind < argc) {
    processFile( argv[optind++] );
  }

  /****
   *
   * we are done
   *
   ****/

  cleanup();

  return( EXIT_SUCCESS );
}

/****
 *
 * display prog info
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

/*****
 *
 * display version info
 *
 *****/

PRIVATE void print_version( void ) {
  printf( "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
}

/*****
 *
 * print help info
 *
 *****/

PRIVATE void print_help( void ) {
  print_version();

  fprintf( stderr, "\n" );
  fprintf( stderr, "syntax: %s [options]\n", PACKAGE );

#ifdef HAVE_GETOPT_LONG
  fprintf( stderr, " -d|--debug (0-9)   enable debugging info\n" );
  fprintf( stderr, " -e|--error (rate)  error rate [default: .01]\n" );
  fprintf( stderr, " -h|--help          this info\n" );
  fprintf( stderr, " -v|--version       display version information\n" );
#else
  fprintf( stderr, " -d (0-9)   enable debugging info\n" );
  fprintf( stderr, " -e (rate)  error rate [default: .01]\n" );
  fprintf( stderr, " -h         this info\n" );
  fprintf( stderr, " -v         display version information\n" );
#endif

  fprintf( stderr, "\n" );
}

/****
 *
 * cleanup
 *
 ****/

PRIVATE void cleanup( void ) {
  int i, j;

  XFREE( config->hostname );
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
  XFREE( config );
}

/****
 * 
 * process files
 * 
 */

/* XXX would be faster to run a read pthread and a bloom pthread */
int processFile( const char *fName ) {
  struct stat fStatBuf;
  size_t fSize;
  struct bloom bf;
  FILE *inFile;
  char rBuf[8192];

  /* get status on file */
  if ( stat( fName, &fStatBuf ) EQ FAILED ) {
    return FAILED;
  }

  /* get size */
  fSize = fStatBuf.st_size;

  /* read first n lines of file */

  /* guess at average line size and overall number of lines in file */
  /* doing this until I add growable bloom filter option */

  /* init bloom filter */
  if ( bloom_init( &bf, fSize / 10, config->eRate ) EQ TRUE ) {
    fprintf( stderr, "ERR - Unable to initialize bloom filter\n" );
    return FAILED;
  }
  bloom_print( &bf );

  /* process all lines in file */

  /* XXX need growable bloom filter to use stdin */
  if ( strcmp( fName, "-" ) EQ 0 ) {
    inFile = stdin;
  } else {
#ifdef HAVE_FOPEN64
    if ( ( inFile = fopen64( fName, "r" ) ) EQ NULL ) {
#else
    if ( ( inFile = fopen( fName, "r" ) ) EQ NULL ) {
#endif
      fprintf( stderr, "ERR - Unable to open file [%s] %d (%s)\n", fName, errno, strerror( errno ) );
      bloom_free( &bf );
      return( EXIT_FAILURE );
    }
  }

  /* XXX need to switch to block reads */
  while ( fgets( rBuf, sizeof( rBuf ), inFile ) != NULL ) {
    if ( !bloom_check_add( &bf, rBuf, strlen( rBuf ) , TRUE ) ) {
      printf( "%s", rBuf );
    }
  }
  /* close file */

  fclose( inFile );
  bloom_free( &bf );
  
  return TRUE;
}
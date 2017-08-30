/*****
 *
 * Copyright (c) 2013-2014, Ron Dilley
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
 * includes
 *.
 ****/

#include <stdio.h>
#include <stdlib.h>

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
      {"logdir", required_argument, 0, 'l' },
      {"md5", no_argument, 0, 'm' },
      {"version", no_argument, 0, 'v' },
      {"debug", required_argument, 0, 'd' },
      {"quick", no_argument, 0, 'q' },
      {"help", no_argument, 0, 'h' },
      {0, no_argument, 0, 0}
    };
    c = getopt_long(argc, argv, "vd:hl:mq", long_options, &option_index);
#else
    c = getopt( argc, argv, "vd:hl:mq" );
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

    case 'h':
      /* show help info */
      print_help();
      return( EXIT_SUCCESS );

    case 'l':
      /* define the dir to store logs in */
      config->log_dir = ( char * )XMALLOC( MAXPATHLEN + 1 );
      XMEMSET( config->log_dir, 0, MAXPATHLEN + 1 );
      XSTRNCPY( config->log_dir, optarg, MAXPATHLEN );

      break;

    case 'm':
      /* md5 hash files */
      config->hash = TRUE;
      break;

    case 'q':
      /* do quick checks only */
      config->quick = TRUE;
      break;

    default:
      fprintf( stderr, "Unknown option code [0%o]\n", c);
    }
  }

  /* set default options */
  if ( config->log_dir EQ NULL ) {
    config->log_dir = ( char * )XMALLOC( strlen( LOGDIR ) + 1 );
    XSTRNCPY( config->log_dir, LOGDIR, strlen( LOGDIR ) );   
  }

  /* turn off quick mode if hash mode is enabled */
  if ( config->hash )
    config->quick = FALSE;

  /* enable syslog */
  openlog( PROGNAME, LOG_CONS & LOG_PID, LOG_LOCAL0 );

  /* check dirs and files for danger */

  if ( time( &config->current_time ) EQ -1 ) {
    display( LOG_ERR, "Unable to get current time" );
    /* cleanup syslog */
    closelog();
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

  //lineHash = initHash( 53 );

  if (optind < argc) {
    processFile( argv[optind++] );
  }

  //freeHash( lineHash );

  /****
   *
   * we are done
   *
   ****/

  /* cleanup syslog */
  closelog();

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
  fprintf( stderr, " -d|--debug (0-9)     enable debugging info\n" );
  fprintf( stderr, " -h|--help            this info\n" );
  fprintf( stderr, " -l|--logdir {dir}    directory to create logs in (default: %s)\n", LOGDIR );
  fprintf( stderr, " -m|--md5             hash files and compare (disables -q|--quick mode)\n" );
  fprintf( stderr, " -q|--quick           do quick comparisons only\n" );
  fprintf( stderr, " -v|--version         display version information\n" );
#else
  fprintf( stderr, " -d {lvl}   enable debugging info\n" );
  fprintf( stderr, " -h         this info\n" );
  fprintf( stderr, " -l {dir}   directory to create logs in (default: %s)\n", LOGDIR );
  fprintf( stderr, " -m         hash files and compare (disables -q|--quick mode)\n" );
  fprintf( stderr, " -q         do quick comparisons only\n" );
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
 * process file
 *
 ****/

#define BLOCK_COUNT 1024

int processFile( const char *fName ) {
  FILE *inFile = NULL, *outFile = NULL;
  unsigned char *inBuf;
  char outFileName[PATH_MAX];
  char oBuf[4096];
  struct stat statBuf;
  PRIVATE int c = 0, i, ret, lineCount = 0;
  size_t rCount = 0, count;
  int inFile_h;
  char lineBuf[8192];
  int linePos = 0, bufPos = 0;
  struct MD5Context ctx;
  unsigned char digest[16];
  uint32_t key;
  fprintf( stderr, "Opening [%s] for read\n", fName );
  if ( strcmp( fName, "-" ) EQ 0 ) {
    inFile = stdin;
  } else {
#ifdef HAVE_FOPEN64
    if ( ( inFile = fopen64( fName, "r" ) ) EQ NULL ) {
#else
    if ( ( inFile = fopen( fName, "r" ) ) EQ NULL ) {
#endif
      fprintf( stderr, "ERR - Unable to open file [%s] %d (%s)\n", fName, errno, strerror( errno ) );
      return( EXIT_FAILURE );
    }
  }

  inFile_h = fileno( inFile );

  /* get the optimal block size */
  if ( fstat( inFile_h, &statBuf ) EQ FAILED ) {
    fprintf( stderr, "ERR - Unable to stat file\n" );
    if ( inFile != stdin )  
      fclose( inFile );
    return FAILED;
  }

  rCount = statBuf.st_blksize * BLOCK_COUNT;

  if ( ( inBuf = XMALLOC( rCount ) ) EQ NULL ) {
    fprintf( stderr, "ERR - Unable to allocate read buffer\n" );
    if ( inFile != stdin )  
      fclose( inFile );
    return FAILED;
  }

  while( ( count = read( inFile_h, inBuf, rCount ) ) > 0 ) {
    bufPos = 0;

    while( bufPos < count ) {
      if ( ( inBuf[bufPos] EQ '\n' ) || ( inBuf[bufPos] EQ '\r' ) ) {
	while( ( bufPos < count ) && ( ( inBuf[bufPos] EQ '\n' ) || ( inBuf[bufPos] EQ '\r' ) ) ) { bufPos++; }
	/* process line */
	if ( linePos > 0 ) {
	  //key = calcBufHash( lineHash->size, lineBuf, linePos );
	  //MD5Init( &ctx );
	  //MD5Update( &ctx, (unsigned char *)lineBuf, linePos );
	  //MD5Final( digest, &ctx );
	  //if ( addUniquePreHashedRec( lineHash, key, digest, 16, NULL ) EQ TRUE ) {
	  //  printf( "%s\n", lineBuf );
	  //}
	  /* reset line buf */
	  linePos = 0;
	}
      } else {
	lineBuf[linePos++] = inBuf[bufPos++];
      }
    }
    //printf( "." );
  }
  /* process leftover bytes */
  if ( linePos > 0 ) {
    /* process line */
    //calcBufHash( lineHash->size, lineBuf, linePos );
    //MD5Init( &ctx );
    //MD5Update( &ctx, (unsigned char *)lineBuf, linePos );
    //MD5Final( digest, &ctx );
    //if ( addUniquePreHashedRec( lineHash, key, digest, 16, NULL ) EQ TRUE ) {
    //  printf( "%s\n", lineBuf );
    //}
    /* reset line buf */
    linePos = 0;
  }
  //printf( "\n" );

  //while( fgets( inBuf, sizeof( inBuf ), inFile ) != NULL & ! quit ) {
  //  if ( reload EQ TRUE ) {
  //    fprintf( stderr, "Processed %d lines/min\n", lineCount );
  //    lineCount = 0;
  //    reload = FALSE;
  //  }
    //printf( "%s",inBuf );
  //}
  
  if ( inFile != stdin )  
    fclose( inFile );

  XFREE( inBuf );

  return TRUE;
}

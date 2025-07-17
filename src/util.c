/*****
 *
 * Description: Utility Functions
 * 
 * Copyright (c) 2009-2025, Ron Dilley
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
 * defines
 *
 ****/

/* turn on priority names */
#define SYSLOG_NAMES

/****
 *
 * includes
 *
 ****/

#include "util.h"

/****
 *
 * local variables
 *
 ****/

PRIVATE char *restricted_environ[] = {
  "IFS= \t\n",
  "PATH= /bin:/usr/bin",
  0
};
PRIVATE char *preserve_environ[] = {
  "TZ",
  0
};

/****
 *
 * external global variables
 *
 ****/

extern Config_t *config;
extern char **environ;

/****
 *
 * functions
 *
 ****/

/****
 *
 * Validates directory security by checking ownership and permissions
 *
 * Traverses the directory path from specified directory to root, checking
 * each component for safe ownership and permissions to prevent privilege
 * escalation attacks. Ensures directories are owned by root or the current
 * user and are not writable by group or other users.
 *
 * Arguments:
 *   dir - Path to the directory to validate
 *
 * Returns:
 *   1 if directory path is secure, 0 if insecure, FAILED (-1) on error
 *
 ****/
int is_dir_safe( const char *dir ) {
  DIR *fd, *start;
  int rc = FAILED;
  char new_dir[PATH_MAX+1];
  uid_t uid;
  struct stat f, l;

  if ( !( start = opendir( "." ) ) ) return FAILED;
  if ( lstat( dir, &l ) == FAILED ) {
    closedir( start );
    return FAILED;
  }
  uid = geteuid();

  do {
    if ( chdir( dir ) EQ FAILED ) break;
    if ( !( fd = opendir( "." ) ) ) break;

#ifdef LINUX
    if ( fstat( dirfd( fd ), &f ) EQ FAILED ) {
#elif defined MACOS
    if ( fstat( fd->__dd_fd, &f ) EQ FAILED ) {
#elif defined CYGWIN
    if ( fstat( fd->__d_fd, &f ) EQ FAILED ) {
#elif defined MINGW
    if ( fstat( fd->dd_handle, &f ) EQ FAILED ) {
#elif defined FREEBSD
    if ( fstat( dirfd( fd ), &f ) EQ FAILED ) { 
#elif defined OPENBSD
    if ( fstat( dirfd( fd ), &f ) EQ FAILED ) { 
#else
    if ( fstat( fd->dd_fd, &f ) EQ FAILED ) {
#endif

      closedir( fd );
      break;
    }
    closedir( fd );

    if ( l.st_mode != f.st_mode || l.st_ino != f.st_ino || l.st_dev != f.st_dev )
      break;
#ifdef MINGW
	if ( f.st_uid && f.st_uid != uid ) {
#else
    if ( ( f.st_mode & ( S_IWOTH | S_IWGRP ) ) || ( f.st_uid && f.st_uid != uid ) ) {
#endif
      rc = 0;
      break;
    }
    dir = "..";
    if ( lstat( dir, &l ) EQ FAILED ) break;
    if ( !getcwd( new_dir, PATH_MAX + 1 ) ) break;
  } while ( new_dir[1] ); /* new_dir[0] will always be a slash */
  if ( !new_dir[1] ) rc = 1;

#ifdef LINUX
  rc = fchdir( dirfd( start ) );
#elif defined MACOS
  rc = fchdir( start->__dd_fd );
#elif defined CYGWIN
  rc = fchdir( start->__d_fd );
#elif defined MINGW
  rc = fchdir( start->dd_handle );
#elif defined FREEBSD
  rc = fchdir( dirfd( start ) );
#elif defined OPENBSD
  rc = fchdir( dirfd( start ) );
#else
  rc = fchdir( start->dd_fd );
#endif

  closedir( start );
  return rc;
}

/****
 *
 * Displays formatted output to appropriate destination based on mode
 *
 * Routes output to syslog in daemon mode or to stdout/stderr in interactive
 * mode. Error messages (LOG_ERR and below) go to stderr, while informational
 * messages go to stdout. Automatically strips trailing newlines and adds
 * priority level prefixes in interactive mode.
 *
 * Arguments:
 *   level - Syslog priority level (LOG_ERR, LOG_INFO, etc.)
 *   format - Printf-style format string
 *   ... - Variable arguments for format string
 *
 * Returns:
 *   TRUE on success, FAILED on error
 *
 ****/

int display( int level, char *format, ... ) {
  PRIVATE va_list args;
  PRIVATE char tmp_buf[SYSLOG_MAX];
  PRIVATE int i;

  va_start( args, format );
  vsprintf( tmp_buf, format, args );
  if ( tmp_buf[strlen(tmp_buf)-1] == '\n' ) {
    tmp_buf[strlen(tmp_buf)-1] = 0;
  }
  va_end( args );

  if ( config->mode != MODE_INTERACTIVE ) {
    /* display info via syslog */
    syslog( level, "%s", tmp_buf );
  } else {
    if ( level <= LOG_ERR ) {
      /* display info via stderr */
      for ( i = 0; prioritynames[i].c_name != NULL; i++ ) {
	if ( prioritynames[i].c_val == level ) {
	  fprintf( stderr, "%s[%u] - %s\n", prioritynames[i].c_name, config->cur_pid, tmp_buf );
	  return TRUE;
	}
      }
    } else {
      /* display info via stdout */
      for ( i = 0; prioritynames[i].c_name != NULL; i++ ) {
	if ( prioritynames[i].c_val == level ) {
	  printf( "%s[%u] - %s\n", prioritynames[i].c_name, config->cur_pid, tmp_buf );
	  return TRUE;
	}
      }
    }
  }

  return FAILED;
}

/****
 *
 * Safely opens a file for writing with security checks
 *
 * Performs secure file opening by first checking if the file exists and
 * validating it's a regular file, then unlinking it and creating a new
 * file with exclusive creation flags to prevent race conditions and
 * symlink attacks. Sets appropriate permissions for the new file.
 *
 * Arguments:
 *   filename - Path to the file to open safely
 *
 * Returns:
 *   File descriptor on success, FAILED (-1) on error
 *
 ****/

static int safe_open( const char *filename ) {
  int fd;
  struct stat sb;
  XMEMSET( &sb, 0, sizeof( struct stat ) );
                                                                 
  if ( lstat(filename, &sb) EQ FAILED ) {
    if (errno != ENOENT)
      return( FAILED );
  } else if ( ( sb.st_mode & S_IFREG) EQ 0 ) {
    errno = EOPNOTSUPP;
    return ( FAILED );
  }

  unlink( filename );
#ifdef MINGW
  fd = open( filename, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR );
#else
  fd = open( filename, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
#endif

  return (fd);
}

/****
 *
 * Removes PID file from filesystem
 *
 * Safely removes the specified PID file if the filename is non-empty.
 * This function is typically called during program shutdown to clean
 * up the PID file created during daemon startup.
 *
 * Arguments:
 *   filename - Path to the PID file to remove
 *
 * Returns:
 *   None (void)
 *
 ****/

static void cleanup_pid_file( const char *filename ) {
  if ( strlen( filename ) > 0 ) {
    unlink( filename );
  }
}

/****
 *
 * Sanitizes the environment variables for security
 *
 * Creates a new restricted environment by preserving only essential
 * environment variables and setting secure defaults. Removes potentially
 * dangerous variables while preserving necessary ones like TZ (timezone).
 * Sets IFS and PATH to safe values to prevent shell injection attacks.
 *
 * Arguments:
 *   None
 *
 * Returns:
 *   None (void)
 *
 ****/

void sanitize_environment( void ) {
  int i;
  char **new_environ;
  char *ptr, *value, *var;
  size_t arr_size = 1;
  size_t arr_ptr = 0;
  size_t len;
  size_t new_size = 0;

  for( i = 0; (var = restricted_environ[i]) != 0; i++ ) {
    new_size += strlen( var ) + 1;
    arr_size++;
  }

  for ( i = 0; (var = preserve_environ[i]) != 0; i++ ) {
    if ( !(value = getenv(var))) continue;
    new_size += strlen( var ) + strlen( value ) + 2;
    arr_size++;
  }

  new_size += ( arr_size * sizeof( char * ) );
  new_environ = (char **)XMALLOC( new_size );
  new_environ[arr_size - 1] = 0;
  ptr = ( char * )new_environ + (arr_size * sizeof(char *));
  for ( i = 0; ( var = restricted_environ[i] ) != 0; i++ ) {
    new_environ[arr_ptr++] = ptr;
    len = strlen( var );
    XMEMCPY( ptr, var, len + 1 );
    ptr += len + 1;
  }

  for ( i = 0; ( var = preserve_environ[i] ) != 0; i++ ) {
    if ( !( value = getenv( var ) ) ) continue;
    new_environ[arr_ptr++] = ptr;
    len = strlen( var );
    XMEMCPY( ptr, var, len );
    *(ptr + len + 1 ) = '=';
    XMEMCPY( ptr + len + 2, value, strlen( value ) + 1 );
    ptr += len + strlen( value ) + 2;
  }

  environ = new_environ;
}

/****
 *
 * Determines if a number is odd
 *
 * Uses modulo operation to check if the given integer is odd.
 * This is an inline function for performance optimization.
 *
 * Arguments:
 *   n - Integer to test for oddness
 *
 * Returns:
 *   Non-zero if number is odd, 0 if even
 *
 ****/

inline int isodd(const int n) {
  return n % 2;
}

/****
 *
 * Determines if a number is even
 *
 * Uses the isodd function to determine if the given integer is even
 * by returning the logical NOT of the odd test result.
 * This is an inline function for performance optimization.
 *
 * Arguments:
 *   n - Integer to test for evenness
 *
 * Returns:
 *   Non-zero if number is even, 0 if odd
 *
 ****/

inline int iseven(const int n) {
  return !isodd(n);
}

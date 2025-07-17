dnl Process this file with autoconf to produce a configure script
AC_PREREQ([2.69])
m4_define([BUNIQ_VERSION], [0.4])
AC_INIT([Bloom Filter Unique], BUNIQ_VERSION, [ron.dilley@uberadmin.com], [buniq], [http://www.uberadmin.com/Projects/buniq/])
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

dnl Checks for programs
AC_PROG_CC

dnl make /usr/local as the default install dir
AC_PREFIX_DEFAULT(/usr/local)

CFLAGS="${CFLAGS} -I. -I.. -I../include `getconf LFS64_CFLAGS`"
LDFLAGS="${LDFLAGS} `getconf LFS64_LDFLAGS` `getconf LFS64_LIBS`"

SPLINT="no"
AC_ARG_ENABLE(splint,
    [  --enable-splint         Debug the code with splint],
	SPLINT="yes"      
	CC="splint -D -I"
    ,)

DEBUG="no"
AC_ARG_ENABLE(debug,
    [  --enable-debug          Enable debugging],
    [ if test "$GCC" = yes; then
          DEBUG="yes"
          CFLAGS="${CFLAGS} -DDEBUG -ggdb"
      else
          DEBUG="yes"
          CFLAGS="${CFLAGS} -DDEBUG"
      fi
    ],)

MEM_DEBUG="no"
AC_ARG_ENABLE(memdebug,
    [  --enable-memdebug       Enable memory debugging],
      MEM_DEBUG="yes"
      CFLAGS="${CFLAGS} -DMEM_DEBUG"
    ,)

SHOW_MEM_DEBUG="no"
AC_ARG_ENABLE(showmemdebug,
    [  --enable-showmemdebug   Enable verbose memory debugging],
      SHOW_MEM_DEBUG="yes"
      MEM_DEBUG="yes"
      CFLAGS="${CFLAGS} -DMEM_DEBUG -DSHOW_MEM_DEBUG"
    ,)

GPROF="no"
AC_ARG_ENABLE(gprof,
    [  --enable-gprof          Enable profiler],
    [ if test "$GCC" = yes; then
          GPROF="yes"
          CFLAGS="${CFLAGS} -DGPROF -pg"
      else
          GPROF="yes"
          CFLAGS="${CFLAGS} -DGPROF"
      fi
    ],)

dnl ############## Security Hardening Options

HARDENING="yes"
AC_ARG_ENABLE(hardening,
    [  --disable-hardening     Disable security hardening],
    [ if test "$enableval" = "no"; then
          HARDENING="no"
      fi
    ],)

if test "$HARDENING" = "yes"; then
    if test "$GCC" = yes; then
        dnl Stack protection
        CFLAGS="${CFLAGS} -fstack-protector-strong"
        CFLAGS="${CFLAGS} -fstack-clash-protection"
        
        dnl Fortify source (requires -O1 or higher)
        CFLAGS="${CFLAGS} -D_FORTIFY_SOURCE=2"
        
        dnl Position Independent Executable
        CFLAGS="${CFLAGS} -fPIE"
        LDFLAGS="${LDFLAGS} -pie"
        
        dnl Relocation Read-Only
        LDFLAGS="${LDFLAGS} -Wl,-z,relro,-z,now"
        
        dnl Security warnings
        CFLAGS="${CFLAGS} -Wall -Wextra -Wformat=2 -Wformat-security"
        CFLAGS="${CFLAGS} -Wstrict-prototypes -Wmissing-prototypes"
        CFLAGS="${CFLAGS} -Wshadow -Wpointer-arith -Wcast-qual"
        CFLAGS="${CFLAGS} -Wwrite-strings -Wmissing-declarations"
        CFLAGS="${CFLAGS} -Wredundant-decls -Wnested-externs"
        CFLAGS="${CFLAGS} -Winline -Wno-long-long -Wunused-variable"
        CFLAGS="${CFLAGS} -Wstrict-aliasing=2 -Wundef"
        
        dnl Buffer overflow protection
        CFLAGS="${CFLAGS} -Warray-bounds -Wformat-overflow=2"
        CFLAGS="${CFLAGS} -Wstringop-overflow=4"
        
        dnl Control flow integrity
        CFLAGS="${CFLAGS} -fcf-protection=full"
        
        dnl Additional security flags (GCC >= 8)
        CFLAGS="${CFLAGS} -fstack-clash-protection"
        
        dnl Remove debug symbols in release builds
        if test "$DEBUG" = "no"; then
            LDFLAGS="${LDFLAGS} -Wl,-s"
        fi
    fi
fi

ASAN="no"
AC_ARG_ENABLE(asan,
    [  --enable-asan           Enable AddressSanitizer],
    [ if test "$GCC" = yes; then
          ASAN="yes"
          CFLAGS="${CFLAGS} -fsanitize=address -fsanitize=undefined"
          CFLAGS="${CFLAGS} -fno-omit-frame-pointer"
          LDFLAGS="${LDFLAGS} -fsanitize=address -fsanitize=undefined"
      fi
    ],)

STATIC_ANALYSIS="no"
AC_ARG_ENABLE(static-analysis,
    [  --enable-static-analysis Enable static analysis warnings],
    [ if test "$GCC" = yes; then
          STATIC_ANALYSIS="yes"
          CFLAGS="${CFLAGS} -fanalyzer"
          CFLAGS="${CFLAGS} -Wanalyzer-too-complex"
          CFLAGS="${CFLAGS} -Wanalyzer-null-dereference"
          CFLAGS="${CFLAGS} -Wanalyzer-use-after-free"
          CFLAGS="${CFLAGS} -Wanalyzer-double-free"
          CFLAGS="${CFLAGS} -Wanalyzer-malloc-leak"
      fi
    ],)

dnl ############# System Dependencies

AC_MSG_CHECKING([for special system dependencies])

AC_CANONICAL_HOST
AC_C_CONST
dnl AC_PROG_GCC_TRADITIONAL is obsolete - removed

dnl Checks for OS type.
case "$host" in
    *-freebsd*)
        AC_DEFINE([BSD_DERIVED],1,BSD_DERIVED)
        AC_DEFINE([FREEBSD],1,FREEBSD)
	AC_DEFINE([__USE_BSD],1,__USE_BSD)
	AC_DEFINE([__FAVOR_BSD],1,__FAVOR_BSD)
	LDFLAGS="${LDFLAGS}"
        ;;
    *-netbsd*)
        AC_DEFINE([BSD_DERIVED],1,BSD_DERIVED)
        AC_DEFINE([NETBSD],1,NETBSD)
	AC_DEFINE([__USE_BSD],1,__USE_BSD)
	AC_DEFINE([__FAVOR_BSD],1,__FAVOR_BSD)
        ;;
    *-openbsd*)
        AC_DEFINE([BSD_DERIVED],1,BSD_DERIVED)
        AC_DEFINE([OPENBSD],1,OPENBSD)
	AC_DEFINE([__USE_BSD],1,__USE_BSD)
	AC_DEFINE([__FAVOR_BSD],1,__FAVOR_BSD)
	AC_DEFINE([_BSD_SOURCE],1,_BSD_SOURCE)
	LDFLAGS="${LDFLAGS}"
        ;;
    *-apple*)
        AC_DEFINE([BSD_DERIVED],1,BSD_DERIVED)
        AC_DEFINE([MACOS],1,MACOS)
        LDFLAGS="${LDFLAGS}"
        AC_SUBST(more_incl)
        more_incl="" 
        ;;
    *-linux*)
        AC_DEFINE([SYSV_DERIVED],1,SYSV_DERIVED)
        AC_DEFINE([LINUX],1,LINUX)
        AC_SUBST(more_incl)
        more_incl=""
	AC_DEFINE([__USE_BSD],1,__USE_BSD)
	AC_DEFINE([__FAVOR_BSD],1,__FAVOR_BSD)
	LDFLAGS="${LDFLAGS}"
        ;;
    *-solaris2.10)
        AC_DEFINE([SYSV_DERIVED],1,SYSV_DERIVED)
        AC_DEFINE([SOLARIS],1,SOLARIS)
	AC_DEFINE([SOLARIS10],1,SOLARIS10)
	LDFLAGS="${LDFLAGS}"
        ;;
    *-solaris*)
        AC_DEFINE([SYSV_DERIVED],1,SYSV_DERIVED)
        AC_DEFINE([SOLARIS],1,SOLARIS)
	LDFLAGS="${LDFLAGS}"
        ;;
    *-aix*)
        AC_DEFINE([SYSV_DERIVED],1,SYSV_DERIVED)
        AC_DEFINE([AIX],1,AIX)
        ;;
    *-hpux*)
        AC_DEFINE([SYSV_DERIVED],1,SYSV_DERIVED)
        AC_DEFINE([HPUX],1,HPUX)
        ;;
esac

dnl ############## Header Checks

AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([libintl.h])
AC_CHECK_HEADERS([memory.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([paths.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([stdbool.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/param.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([wchar.h])
AC_CHECK_HEADERS([ftw.h])
AC_CHECK_HEADER_STDBOOL

dnl ############## Function checks

AC_CHECK_FUNCS([getopt_long])
AC_CHECK_FUNCS([getopt])
AC_CHECK_FUNCS([bzero])
AC_CHECK_FUNCS([endpwent])
AC_CHECK_FUNCS([fchdir])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([gethostname])
AC_CHECK_FUNCS([opendir])
AC_CHECK_FUNCS([nftw])
AC_CHECK_FUNCS([strncat])
AC_CHECK_FUNCS([strlcat])
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC

dnl Checks for headers

dnl ############## Type checks

AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_UINT8_T
AC_TYPE_INT8_T
AC_TYPE_UINT64_T

AC_CONFIG_HEADERS([include/config.h])
AC_PROG_INSTALL
AC_CONFIG_FILES([Makefile src/Makefile buniq.1])
AC_OUTPUT

BINDIR=`eval echo ${bindir}`; BINDIR=`eval echo ${BINDIR}`;
MANDIR=`eval echo ${mandir}/${mansubdir}`; MANDIR=`eval echo ${MANDIR}`;

echo ""
echo "-----------------------------------------------"
echo "Host type             : ${host}"
echo "Compiler              : ${CC}"
echo "Compiler flags        : ${CFLAGS}"
echo "Linker flags          : ${LDFLAGS}"
echo "Libraries             : ${LIBS}"
echo ""
echo "Binary                : ${BINDIR}"
echo "Manual pages          : ${MANDIR}"
echo ""
echo "Enable debugging      : ${DEBUG}"
echo "Enable mem debugging  : ${MEM_DEBUG}"
echo "Show mem debugging    : ${SHOW_MEM_DEBUG}"
echo "Security hardening    : ${HARDENING}"
echo "AddressSanitizer      : ${ASAN}"
echo "Static analysis       : ${STATIC_ANALYSIS}"
echo "-----------------------------------------------"
echo ""

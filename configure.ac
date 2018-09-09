# Configure template for RCS

# Copyright (C) 2010-2012 Thien-Thi Nguyen
# Copyright (C) 1995 Paul Eggert
#
# This file is part of GNU RCS.
#
# GNU RCS is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNU RCS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([GNU RCS],
	m4_esyscmd([build-aux/git-version-gen build-aux/.version 's/^/v/']),
	[bug-rcs@gnu.org])
AC_CONFIG_HEADERS([lib/config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11.1 readme-alpha no-define])
AS_CASE([$PACKAGE_VERSION],[*.*.*],[pretest_p=yes],[pretest_p=no])

# syntax terrors

AC_DEFUN([RCS_YESP],[test yes = "$[]$1"])dnl $1 -- shell variable name

AC_DEFUN([RCS_NORM],[m4_split(m4_normalize($1))])dnl

AC_DEFUN([RCS_ALL],[m4_map_sep([$1],[ &&
     ],RCS_NORM($2))])dnl

AC_DEFUN([RCS_CBOOL],[dnl
dnl $1 -- C variable (actually #define) name
dnl $2 -- sense that maps to "1" (yes, no, set, etc.)
dnl $3 -- shell variable name to check
dnl $4 -- description for the config header file
AS_IF([test $2 = $[]$3],[cbool=1],[cbool=0])
AC_DEFINE_UNQUOTED([$1],[$cbool],[$4])dnl
])dnl

AC_DEFUN([RCS_SET_VN],[dnl
dnl $1 -- shell variable name
dnl $2 -- value to assign if last command was successful
rv=$?
AS_IF([test 0 = $rv],[$1=$2],[$1=no])
AS_SET_STATUS([$rv])
])dnl

AC_DEFUN([RCS_MUSTHAVE],[dnl
dnl $1 -- shell variable name
dnl $2 -- rest of "could not find ..." error message
AS_IF([test -z "$[]$1" || test no = "$[]$1"],[AC_MSG_ERROR([Could not find $2])])
])dnl

AC_DEFUN([RCS_ABSOLUTE_PROGRAM],[dnl
dnl $1 -- shell variable name
dnl $2 -- class of program
dnl $3 -- description for config header file
save_$1=$[]$1
AC_PATH_PROG([$1],[$[]$1],[no])
RCS_MUSTHAVE([$1],[absolute name of $2 program: $save_$1])
AC_DEFINE_UNQUOTED([$1],["$[]$1"],[$3])
])dnl

# --enable, --with

AC_ARG_ENABLE([coverage],
 [AS_HELP_STRING([--enable-coverage],[if the compiler is GCC, arrange
   to include --coverage in CFLAGS and to use exit(3) instead of _Exit(3);
   this option is intended for maintainers])],,[enable_coverage=no])

AC_ARG_ENABLE([compat2],
 [AS_HELP_STRING([--enable-compat2],[enable reading of RCS 2.x
   files (really ancient, prior to GNU RCS).  If you enable this,
   consider hiring ttn to migrate your files to a format more
   suited to this millenium!  (The file format output by RCS 2.x
   became obsolete in 1982.)])],,[enable_compat2=no])
RCS_CBOOL([COMPAT2],[yes],[enable_compat2],
  [Support reading RCS 2.x files?])

AC_ARG_WITH([mailer],
 [AS_HELP_STRING([--with-mailer=PROG],[use PROG to send mail
   when a lock is broken (default: don't send mail)])],,[with_mailer=no])
AS_IF([test no = "$with_mailer"],[:],[
SENDMAIL="$with_mailer"
RCS_ABSOLUTE_PROGRAM([SENDMAIL],[mailer],
  [Program used to send mail when a lock is broken.])
])

AC_ARG_ENABLE([mmap],
 [AS_HELP_STRING([--disable-mmap],[don't use mmap(2), ever
   (default: use mmap(2) if it seems sane enough)])],
 [use_mmap=$enableval],[use_mmap=yes])

AC_ARG_ENABLE([suid],
 [AS_HELP_STRING([--enable-suid[[=setreuid]]],[enable setuid
   operation, preferring setreuid(2) if available over seteuid(2)
   if optional arg `setreuid' is specified (default: enable
   setuid operation using seteuid(2))])],[:],[enable_suid=yes])

# Set up simple `diff' test.
echo 0 >conftest0
echo 0 >conftest0c
echo 1 >conftest1
{ echo d1 1 ; echo a1 1 ; echo 1 ; } >conftestok

# Set DIFF to the name of the `diff' program to be run.
AC_DEFUN([RCS_CHECK_DIFF],[dnl
dnl $1 -- shell variable name
dnl $2 -- program
AS_IF([RCS_YESP([sane_diff])],[:],[
AC_CACHE_CHECK([if $2 works like GNU diff],[$1],[
  $1=no
  sh -c "exec $2 -n conftest0 conftest1" >conftestout 2>/dev/null
  test 1 = $? && cmp -s conftestok conftestout
  RCS_SET_VN([$1],[yes])
])
AS_IF([RCS_YESP([$1])],[
  sane_diff=yes
  DIFF=$2
])])])dnl
AC_DEFUN([DIFF_CLASS],[line-by-line differences])dnl
AC_DEFUN([DIFF_MAYBE],[gdiff rdiff diff])
AC_ARG_VAR([DIFF],[the name of a program that works like diff(1)
  from GNU diffutils, used to compare files line by line
  (default is to search the PATH for: ]DIFF_MAYBE[)])

sane_diff=no
AS_IF([test "$DIFF"],[RCS_CHECK_DIFF([rcs_cv_sane_DIFF],[$DIFF])])
m4_foreach([prog],RCS_NORM([DIFF_MAYBE]),[
RCS_CHECK_DIFF([rcs_cv_sane_]prog,prog)
])

RCS_MUSTHAVE([sane_diff],[a sane DIFF_CLASS program (tried: DIFF_MAYBE)])
RCS_ABSOLUTE_PROGRAM([DIFF],[DIFF_CLASS],
  [The diff program from GNU diffutils, or workalike.])

# Set DIFF_SUCCESS, DIFF_FAILURE, DIFF_TROUBLE to diff's exit status
# when it finds no differences, some differences, or trouble.
AC_DEFUN([RCS_CHECK_STATUS],[
AS_VAR_PUSHDEF([SDV],[rcs_cv_status_diff_$1])dnl
AS_VAR_PUSHDEF([CVAR],[DIFF_]m4_toupper($1))dnl
AC_CACHE_CHECK([diff $1 status],[SDV],[
  $DIFF $3 $4 >/dev/null 2>&1
  SDV=$?
])
AC_DEFINE_UNQUOTED([CVAR],[$[]SDV],
  [Exit status when diff(1) finds $2.])
AS_VAR_POPDEF([CVAR])dnl
AS_VAR_POPDEF([SDV])dnl
])

# We can't use `$DIFF conftest0 conftest0',
# since buggy NEXTSTEP 3.0 diff silently yields exit status 2 for this.
RCS_CHECK_STATUS([success],[no differences],[conftest0],[conftest0c])
RCS_CHECK_STATUS([failure],[some differences],[conftest0],[conftest1])
RCS_CHECK_STATUS([trouble],[trouble],[conftest0],[no/such/file])

# Set DIFFFLAGS to the options of the `diff' program to be run.
# Use -an if possible, -n otherwise.
AC_CACHE_CHECK([diff options for RCS],[rcs_cv_options_diff],[
  $DIFF -an conftest0 conftest1 >conftestout 2>conftestout2
AS_IF([test 1 = $? &&
       cmp -s conftestok conftestout &&
       test ! -s conftestout2],
      [rcs_cv_options_diff=-an],
      [rcs_cv_options_diff=-n])
])
AC_DEFINE_UNQUOTED([DIFFFLAGS],["$rcs_cv_options_diff"],
  [Options of the `diff' program to be run.])

# Set DIFF_L to 1 if diff understands the ‘--label’ option, 0 otherwise.
AC_CACHE_CHECK([if diff understands --label],[rcs_cv_options_diff_l],[
  $DIFF -c --label 0 --label 1 conftest0 conftest1 >conftestout 2>/dev/null
  test 1 = $? && cmp -s - conftestout <<'EOF'
*** 0
--- 1
***************
*** 1 ****
! 0
--- 1 ----
! 1
EOF
  RCS_SET_VN([rcs_cv_options_diff_l],[yes])
])
RCS_CBOOL([DIFF_L],[yes],[rcs_cv_options_diff_l],
  [Does diff(1) understand `--label'?])

# Set DIFF3 to the name of the diff3 program.
AC_DEFUN([DIFF3_MAYBE],[gdiff3 diff3])
AC_ARG_VAR([DIFF3],[the name of a program that works like diff3(1)
  from GNU diffutils, used to compare three files line by line
  (default is to search the PATH for: ]DIFF3_MAYBE[)])
AC_CHECK_PROGS([DIFF3],[DIFF3_MAYBE])
RCS_MUSTHAVE([ac_cv_prog_DIFF3],[a diff3 program (tried: DIFF3_MAYBE)])
RCS_ABSOLUTE_PROGRAM([DIFF3],[diff3],
  [A diff3 program, possibly slightly insane wrt GNU diff3.])

AC_CACHE_CHECK([if diff3 works like GNU diff3],[rcs_cv_sane_diff3],[
  rcs_cv_sane_diff3=no
  sh -c "exec $DIFF3 -E -m -L 0 -L 1 -L 2 conftest0 conftest1 /dev/null" >conftestout 2>/dev/null
  test 1 = $? && cmp -s - conftestout <<'EOF'
<<<<<<< 0
0
=======
>>>>>>> 2
EOF
  RCS_SET_VN([rcs_cv_sane_diff3],[yes])
])
RCS_CBOOL([DIFF3_BIN],[yes],[rcs_cv_sane_diff3],
  [Does diff3 work like GNU binutils diff3?])
AS_IF([RCS_YESP([rcs_cv_sane_diff3])],,[
ED=ed
RCS_ABSOLUTE_PROGRAM([ED],[original ed],
  [The singular ed(1), necessary only if diff3 is not completely sane.])
])

# This is only for maintainers.
AC_ARG_VAR([SOELIM], [the name of the soelim(1) program, typically
  from groff, used to process .so directives in manpage source;
  useful only if you edit man/*.[15]in or make `maintainer-clean'])
AS_IF([test x"$SOELIM" = x],[AC_CHECK_PROGS([SOELIM], [soelim])])

AC_ARG_VAR([PIC], [the pic(1) command; if unspecified, configure
  tries (GNU groff) "pic -n", which avoids GNU extensions (needed
  for proper operation to generate a portable man page); otherwise,
  these are tried in order: "gpic -n", "pic -D" (traditional), "pic"])
AS_IF([test x"$PIC" = x],
      [AC_CHECK_PROGS([PIC],
                      ["pic -n" "gpic -n" "pic -D" "pic"],
                      [pic])])

AC_PROG_CC_C99
# The var ‘ac_cv_prog_cc_c99’ records options required to place compiler
# in "C99 mode".  It may be empty if that is the default compiler mode.
AS_IF([test x"$ac_cv_prog_cc_c99" = x],[:],
      [RCS_MUSTHAVE([ac_cv_prog_cc_c99],[a C99-capable compiler])])
AS_IF([test x"$EXEEXT" = x],[:],
  [AC_DEFINE_UNQUOTED([EXEEXT],["$EXEEXT"],
     [The extension (including leading dot) of executable files.])])

# gnulib
gl_EARLY
gl_INIT

# headers
AC_CHECK_HEADERS_ONCE([
  limits.h mach/mach.h net/errno.h
  pwd.h
  siginfo.h utime.h
])
AS_IF([RCS_YESP([use_mmap])],[AC_CHECK_HEADERS([sys/mman.h])])

# types -- see also: (info "(autoconf) Particular Types")
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T

# funcs

AC_FUNC_FORK
AC_CHECK_FUNCS_ONCE([
  fchmod
  ftruncate
  getpwuid_r
  psiginfo
])
AS_IF([RCS_YESP([use_mmap])],[AC_CHECK_FUNCS([mmap madvise])])
AS_IF([test xsetreuid = x"$enable_suid"],[enable_suid=yes
  AC_CHECK_FUNCS([setreuid])])
AS_IF([RCS_YESP([enable_suid])],[AC_CHECK_FUNCS([getuid seteuid setuid])])

# system services

# specific behaviors (that unfortunately require running a program to check)

AC_CACHE_CHECK([if tinysym init includes NUL],[rcs_cv_tinyinit_ok],[
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <stdint.h>

struct tinysym
{
  uint8_t len;
  uint8_t bytes[];
};

#define TINY(x)       (tiny_ ## x)
#define TINY_DECL(x)  const struct tinysym (TINY (x))
#define TINY_INIT(x)  { .len = sizeof (x) - 1, .bytes = x }
#define TINYK(x)      TINY_DECL (x) = TINY_INIT (#x)

/* Apparently, SunOS 5.11 compilers cc and c99 omit the '\0'
   from these data initialization forms.  */
TINYK (access);
TINYK (integrity);
]],[[
  const struct tinysym *one = &TINY (access);
  const struct tinysym *two = &TINY (integrity);

  if (6 != one->len || '\0' != one->bytes[6])
    return 1;
  if (9 != two->len || '\0' != two->bytes[9])
    return 2;
]])],[rcs_cv_tinyinit_ok=yes],[rcs_cv_tinyinit_ok=no])
])
RCS_CBOOL([TINY_INIT_NEEDS_EXPLICIT_NUL],[no],[rcs_cv_tinyinit_ok],
  [Does the compiler forget to include '\0' for tinysym init?])

AC_CACHE_CHECK([if `fopen (F, "w+")' truncates],[rcs_cv_fopen_truncates],[
  echo nonempty > conftest.data
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
]],[[
  return ! fopen ("conftest.data", "w+");
]])
],[res=yes],[res=no])
  test yes = $res && test ! -s conftest.data || res=no
  rcs_cv_fopen_truncates=$res
])
RCS_CBOOL([BAD_FOPEN_WPLUS],[no],[rcs_cv_fopen_truncates],
  [Does `fopen (F, "w+")' fail to truncate?])

AC_CACHE_CHECK([if `write' after `creat (F, 0)' works],[rcs_cv_wac_ok],[
  rm -f conftest.data
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined O_CREAT && defined O_WRONLY
# define creat0(f) open(f, O_CREAT|O_WRONLY, 0)
#else
# define creat0(f) creat(f, 0)
#endif
char buf[17000];
]],[[
  int f;
  return (0 > (f = creat0 ("conftest.data"))
          || sizeof (buf) != write (f, buf, sizeof (buf))
          || 0 > close (f));
]])
],[res=yes],[res=no])
  if test yes = "$res" && test -f conftest.data && test ! -w conftest.data
    then :
    else res=no
  fi
  rcs_cv_wac_ok=$res
])
RCS_CBOOL([BAD_CREAT0],[no],[rcs_cv_wac_ok],
  [Does `write' after `creat (F, 0)' fail in the wrong way?])

AC_CACHE_CHECK([if `unlink' works on unwritable files],[rcs_cv_unlink_mw_ok],[
  touch conftest.data
  chmod -w conftest.data
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
]],[
  return 0 > unlink ("conftest.data");
])
],[res=yes],[res=no])
  test yes = $res && test -f conftest.data && res=no
  rcs_cv_unlink_mw_ok=$res
])
RCS_CBOOL([BAD_UNLINK],[no],[rcs_cv_unlink_mw_ok],
  [Does `unlink' fail on unwritable files?])

AC_CACHE_CHECK([if `rename' handles unwritable source/target],
               [rcs_cv_rename_mw],[
m4_pushdef([ZONK],[rm -f conftest-A conftest-B && echo A > conftest-A])dnl
m4_pushdef([GOOD],[test ! -f conftest-A && test -f conftest-B])dnl
m4_pushdef([HMMM],[./conftest$EXEEXT && GOOD])dnl
  ZONK
  res=hopeless
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
]],[
  return 0 > rename ("conftest-A", "conftest-B");
])
],[
  if GOOD ; then
    res=ok
    # unwritable source
    ZONK ; chmod -w conftest-A
    HMMM && AS_VAR_APPEND([res],[,source=ok])
    # unwritable target
    ZONK ; echo B > conftest-B ; chmod -w conftest-B
    HMMM && AS_VAR_APPEND([res],[,target=ok])
  fi
])
  rcs_cv_rename_mw=$res
m4_popdef([HMMM])dnl
m4_popdef([GOOD])dnl
m4_popdef([ZONK])dnl
])
rename_source=no
rename_target=no
AS_IF([test hopeless = $rcs_cv_rename_mw],,
  [eval `echo $rcs_cv_rename_mw | sed 's/^..//;s/,/ rename_/g'`])
RCS_CBOOL([BAD_A_RENAME],[no],[rename_source],
  [Does `rename' fail if the source is unwritable?])
RCS_CBOOL([BAD_B_RENAME],[no],[rename_target],
  [Does `rename' fail if the target is unwritable?])

AC_DEFUN([SIMPLE_MMAP_PROGRAM],[AC_LANG_PROGRAM([[
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
]],[
  struct stat b;
  char *a;
  int fd, fc;

  fd = open ("conftest.data", O_RDONLY);
  fstat (fd, &b);
  a = mmap (NULL, b.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if ((char *) MAP_FAILED == a)
    {
      perror ("mmap");
      return 1;
    }
  /* Finish.  */
$1
])])dnl

AS_IF([RCS_ALL([RCS_YESP],[
  use_mmap
  ac_cv_func_mmap
  ac_cv_header_sys_mman_h
])],[
AC_CACHE_CHECK([that `mmap' is sane],[rcs_cv_sane_mmap],[
  rcs_cv_sane_mmap=yes
  # AIX 3.2.0 read-only mmap updates last-modified time of file!
  if uname -a | grep AIX >/dev/null ; then
    echo nonempty > conftest.data
    bef=`date -r conftest.data`
    sleep 2
    AC_RUN_IFELSE([SIMPLE_MMAP_PROGRAM])
    aft=`date -r conftest.data`
    test "$bef" = "$aft" || rcs_cv_sane_mmap=no
  fi
])
AS_IF([RCS_YESP([rcs_cv_sane_mmap])],[
AC_CACHE_CHECK([signal received if referencing nonexistent part of mmapped file],
               [rcs_cv_mmap_signal],[
    res=SIGBUS
    echo nonempty > conftest.data
    AC_RUN_IFELSE([SIMPLE_MMAP_PROGRAM([dnl
      fc = creat ("conftest.data", 0);
      /* Refer to nonexistent storage, causing a signal.  */
      printf ("%d %c\n", fc, *a);
])],[res=unknown],[res=$?])
    # If there is no signal, better to disable mmap entirely.
    AS_IF([test unknown = $res],[res=0],
          [test SIGBUS = $res],[:],
          [test 128 -ge $res],[res=SIGBUS],
          [AS_VAR_ARITH([res],[$res - 128])])
    rcs_cv_mmap_signal=$res
])
])])
AC_DEFINE_UNQUOTED([MMAP_SIGNAL],[${rcs_cv_mmap_signal-0}],
  [Signal received if referencing nonexistent part of mmapped file.])

AC_CACHE_CHECK([if `wait' can handle ignored SIGCHLD],[rcs_cv_tolerant_wait],[
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
]],[
  signal (SIGCHLD, SIG_IGN);
#ifdef HAVE_WORKING_FORK
  {
    int status;
    pid_t p = fork ();
    if (p < 0)
      {
        perror ("fork");
        return 2;
      }
    if (!p)
      _exit (0);
    while (wait (&status) != p)
      {
        if (ECHILD == errno)
          return 1;
        if (EINTR != errno)
          {
            perror ("wait");
            return 2;
          }
      }
  }
#else
  if (0 > system ("true"))
    return 1;
#endif
])dnl
],[res=yes],[res=no])
  rcs_cv_tolerant_wait=$res
])
RCS_CBOOL([BAD_WAIT_IF_SIGCHLD_IGNORED],[no],[rcs_cv_tolerant_wait],
  [Does ignoring SIGCHLD break `wait'?])

# miscellaneous

AS_IF([RCS_YESP([GCC])],,[enable_coverage=no])
AS_IF([RCS_YESP([enable_coverage])],[CFLAGS="$CFLAGS --coverage"])
RCS_CBOOL([USE_NORMAL_EXIT],[yes],[enable_coverage],
 [Use exit(3) instead of _Exit(3).])

AS_IF([test 1 = `expr "$ac_pwd" : '/*'`],[woe=no],[woe=yes])
RCS_CBOOL([WOE],[yes],[woe],[Are we trying to "support" MS-DOS?])

AC_DEFUN([WOEMUX],[AS_IF([test yes = $woe],[$2],[$1])])

WOEMUX([ABSFILENAMEGLOB='/*'],[ABSFILENAMEGLOB='?:*'])
AC_SUBST([ABSFILENAMEGLOB])

# Encourage user feedback for weird systems.
warn_weird_p=yes

AC_DEFUN([PRESUMABLY_STANDARD_ENOUGH_BY_NOW],[dnl

dnl These can probably be handled by gnulib, no sweat.
ac_cv_header_utime_h            dnl SVr4, POSIX.1-2001
ac_cv_header_pwd_h              dnl 4.3BSD, SVr4, POSIX.1-2001
ac_cv_func_fchmod               dnl 4.4BSD, SVr4, POSIX.1-2001
ac_cv_func_ftruncate            dnl 4.4BSD, SVr4, POSIX.1-2001

dnl These are things we would like to not worry about.
dnl Removing checks for them (assuming yes) eases maintenance.
dnl Because their removal is motivated primarily by laziness,
dnl and only secondarily by standards compliance, we should not
dnl be surprised if there is some resistence from pretest users.
dnl In that case, fine, just keep them (remove them from this list).
rcs_cv_fopen_truncates
rcs_cv_unlink_mw_ok
rcs_cv_wac_ok

dnl Some things to think about
dnl mmap_signal_is_SIGBUS -- TODO: make rcs_cv_mmap_signal have symbolic
dnl                          value, not just numeric (like old src/conf.h)
])

AS_IF([RCS_YESP([warn_weird_p])],[
  AS_IF([RCS_ALL([RCS_YESP],[PRESUMABLY_STANDARD_ENOUGH_BY_NOW])],[:],
   [AC_MSG_WARN([Your platform might be desupported in the future:
m4_foreach([V],RCS_NORM([PRESUMABLY_STANDARD_ENOUGH_BY_NOW]),[dnl
	${V-(unset)}	V
])])])])

# write it out

AC_CONFIG_FILES([
  lib/Makefile
  Makefile
  doc/Makefile
  man/Makefile
  src/Makefile
  tests/Makefile
])

# We don't use or install rcsfreeze, so there is no point making it executable.
AC_CONFIG_FILES([src/rcsfreeze:src/rcsfreeze.sh])

# We could do this "manually" in tests/Makefile.am, specifying
# another file for ‘check_SCRIPTS’ and writing a custom target,
# all to save the configure script from processing a possibly
# unused file (in the case the user does not do "make check").
# But it's not much of a burden, and users SHOULD be doing
# "make check" anyway.
AC_CONFIG_FILES([tests/known-failures],
       [chmod +x tests/known-failures])

AC_OUTPUT

# configure.ac ends here

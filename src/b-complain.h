/* b-complain.h --- various ways of writing to standard error

   Copyright (C) 2010-2012 Thien-Thi Nguyen

   This file is part of GNU RCS.

   GNU RCS is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU RCS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

extern void unbuffer_standard_error (void);
extern void vcomplain (char const *fmt, va_list args);
extern void complain (char const *fmt, ...)
  printf_string (1, 2);
extern void diagnose (char const *fmt, ...)
  printf_string (1, 2);
extern void syserror (int e, char const *who);
extern void generic_warn (char const *who, char const *fmt, ...)
  printf_string (2, 3);
extern void generic_error (char const *who, char const *fmt, ...)
  printf_string (2, 3);
extern void generic_fatal (char const *who, char const *fmt, ...)
  printf_string (2, 3) exiting;
extern void fatal_syntax (size_t lno, char const *fmt, ...)
  printf_string (2, 3) exiting;
extern void fatal_sys (char const *who)
  exiting;

/* Idioms.  Here, prefix P stands for "program" (general operation);
   M for "manifestation"; R for "repository".  */

#define syserror_errno(who)  syserror (errno, who)

#define PWARN(...)     generic_warn (NULL, __VA_ARGS__)
#define MWARN(...)     generic_warn (MANI (filename), __VA_ARGS__)
#define RWARN(...)     generic_warn (REPO (filename), __VA_ARGS__)

#define PERR(...)      generic_error (NULL, __VA_ARGS__)
#define MERR(...)      generic_error (MANI (filename), __VA_ARGS__)
#define RERR(...)      generic_error (REPO (filename), __VA_ARGS__)

#define PFATAL(...)    generic_fatal (NULL, __VA_ARGS__)
#define RFATAL(...)    generic_fatal (REPO (filename), __VA_ARGS__)

#define SYNTAX_ERROR(...)  fatal_syntax (0, __VA_ARGS__)

/* b-complain.h ends here */

/* b-fb.c --- basic file operations

   Copyright (C) 2010-2012 Thien-Thi Nguyen
   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Copyright (C) 1982, 1988, 1989 Walter Tichy

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

#include "base.h"
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include "unistd-safer.h"
#include "b-complain.h"

int
change_mode (int fd, mode_t mode)
{
#ifndef HAVE_FCHMOD
  return -1;
#else
  return fchmod (fd, mode);
#endif
}

void
Ierror (void)
{
  fatal_sys ("input error");
}

void
testIerror (FILE *f)
{
  if (ferror (f))
    Ierror ();
}

void
Oerror (void)
{
  if (BE (Oerrloop))
    BOW_OUT ();
  BE (Oerrloop) = true;
  fatal_sys ("output error");
}

void
testOerror (FILE *o)
{
  if (ferror (o))
    Oerror ();
}

FILE *
fopen_safer (char const *filename, char const *type)
/* Like ‘fopen’, except the result is never stdin, stdout, or stderr.  */
{
  FILE *stream = fopen (filename, type);

  if (stream)
    {
      int fd = fileno (stream);

      if (STDIN_FILENO <= fd && fd <= STDERR_FILENO)
        {
          int f = dup_safer (fd);

          if (PROB (f))
            {
              int e = errno;

              fclose (stream);
              errno = e;
              return NULL;
            }
          if (PROB (fclose (stream)))
            {
              int e = errno;

              close (f);
              errno = e;
              return NULL;
            }
          stream = fdopen (f, type);
        }
    }
  return stream;
}

void
Ozclose (FILE **p)
{
  if (*p && EOF == fclose (*p))
    Oerror ();
  *p = NULL;
}

void
aflush (FILE *f)
{
  if (PROB (fflush (f)))
    Oerror ();
}

void
oflush (void)
{
  FILE *mstdout = MANI (standard_output);

  if (PROB (fflush (mstdout
                    ? mstdout
                    : stdout))
      && !BE (Oerrloop))
    Oerror ();
}

void
afputc (int c, register FILE *f)
/* ‘afputc (c, f)’ acts like ‘aputc (c, f)’ but is smaller and slower.  */
{
  aputc (c, f);
}

void
aputs (char const *s, FILE *iop)
/* Put string ‘s’ on file ‘iop’, abort on error.  */
{
  if (PROB (fputs (s, iop)))
    Oerror ();
}

void
aprintf (FILE * iop, char const *fmt, ...)
/* Formatted output.  Same as ‘fprintf’ in <stdio.h>,
   but abort program on error.  */
{
  va_list ap;

  va_start (ap, fmt);
  if (PROB (vfprintf (iop, fmt, ap)))
    Oerror ();
  va_end (ap);
}

/* b-fb.c ends here */

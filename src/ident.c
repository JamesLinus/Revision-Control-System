/* Identify RCS keyword strings in files.

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
#include <errno.h>
#include <stdlib.h>
#include "ident.help"
#include "b-complain.h"

struct top *top;

static int
match (register FILE *fp)
/* Group substring between two KDELIM's; then do pattern match.  */
{
  char line[BUFSIZ];
  register int c;
  register char *tp;
  bool svn_p = false;

  /* For Subversion-style fixed-width keyword format accept the extra
     colon and allow for a hash immediately before the end ‘KDELIM’
     in that case (e.g., "$KEYWORD:: TEXT#$").
     ------------------------------^     ^----- (maybe)  */

  tp = line;
  while ((c = getc (fp)) != VDELIM)
    {
      if (c == EOF && feof (fp) | ferror (fp))
        return c;
      switch (ctab[c])
        {
        case LETTER:
        case Letter:
          *tp++ = c;
          if (tp < line + sizeof (line) - 4)
            break;
          /* fall into */
        default:
           /* Anything but 0 or KDELIM or EOF.  */
          return c ? c : '\n';
        }
    }
  if (tp == line)
    return c;
  *tp++ = c;
  if (':' == (c = getc (fp)))
    {
      svn_p = true;
      *tp++ = c;
      c = getc (fp);
    }
  if (c != ' ')
    return c ? c : '\n';
  *tp++ = c;
  while ((c = getc (fp)) != KDELIM)
    {
      if (c == EOF && feof (fp) | ferror (fp))
        return c;
      switch (ctab[c])
        {
        default:
          *tp++ = c;
          if (tp < line + sizeof (line) - 2)
            break;
          /* fall into */
        case NEWLN:
        case UNKN:
          return c ? c : '\n';
        }
    }
  /* Sanity check: The end is ' ' (or possibly '#' for svn)?  */
  if (! (' ' == tp[-1]
         || (svn_p && '#' == tp[-1])))
    return c;
  /* Append trailing KDELIM.  */
  *tp++ = c;
  *tp = '\0';
  printf ("     %c%s\n", KDELIM, line);
  return 0;
}

static int
scanfile (register FILE *file, char const *name)
/* Scan an open ‘file’ (perhaps with ‘name’) for keywords.  Return
   -1 if there's a write error; exit immediately on a read error.  */
{
  register int c;

  if (name)
    {
      printf ("%s:\n", name);
      if (ferror (stdout))
        return -1;
    }
  else
    name = "standard input";
  c = 0;
  while (c != EOF || !(feof (file) | ferror (file)))
    {
      if (c == KDELIM)
        {
          if ((c = match (file)))
            continue;
          if (ferror (stdout))
            return -1;
          BE (quiet) = true;
        }
      c = getc (file);
    }
  if (ferror (file) || PROB (fclose (file)))
    {
      syserror_errno (name);
      /* The following is equivalent to ‘exit (EXIT_FAILURE)’, but we
         invoke ‘BOW_OUT’ to keep lint, as well as the DOS and OS/2 ports
         happy.  [Is this still relevant? --ttn]  */
      fflush (stdout);
      BOW_OUT ();
    }
  if (!BE (quiet))
    complain ("%s warning: no id keywords in %s\n", PROGRAM (name), name);
  return 0;
}

int
main (int argc, char **argv)
{
  FILE *fp;
  int status = EXIT_SUCCESS;
  char const *a;
  const struct program program =
    {
      .invoke = argv[0],
      .name = "ident",
      .help = ident_help,
      .tyag = TYAG_IMMEDIATE
    };

  CHECK_HV ();
  gnurcs_init (&program);

  while ((a = *++argv) && *a == '-')
    while (*++a)
      switch (*a)
        {
        case 'q':
          BE (quiet) = true;
          break;

        case 'V':
          display_version (&program);
          gnurcs_goodbye ();
          return EXIT_SUCCESS;

        default:
          bad_option (a - 1);
          gnurcs_goodbye ();
          return EXIT_FAILURE;
          break;
        }

  if (!a)
    scanfile (stdin, NULL);
  else
    do
      {
        if (!(fp = fopen (a, FOPEN_RB)))
          {
            syserror_errno (a);
            status = EXIT_FAILURE;
          }
        else if (PROB (scanfile (fp, a))
                 || (argv[1] && putchar ('\n') == EOF))
          break;
      }
    while ((a = *++argv));

  if (ferror (stdout) || PROB (fclose (stdout)))
    {
      syserror_errno ("standard output");
      status = EXIT_FAILURE;
    }
  gnurcs_goodbye ();
  return status;
}

/*:help
[options] [file ...]
Options:
  -q            Suppress warnings if no patterns are found.
  -V            Like --version.

If no FILE is specified, scan standard input.
*/

/* ident.c ends here */

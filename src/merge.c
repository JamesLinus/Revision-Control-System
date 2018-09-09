/* Three-way file merge.

   Copyright (C) 2010-2012 Thien-Thi Nguyen
   Copyright (C) 1991, 1992, 1993, 1994, 1995 Paul Eggert

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
#include <stdlib.h>
#include "merge.help"
#include "b-complain.h"
#include "b-feph.h"
#include "b-merger.h"

struct top *top;

int
main (int argc, char **argv)
{
  register char const *a;
  struct symdef three_manifestations[3];
  char const *edarg = NULL;
  int labels, exitstatus;
  bool tostdout = false;
  const struct program program =
    {
      .invoke = argv[0],
      .name = "merge",
      .help = merge_help,
      .tyag = BOG_DIFF
    };

  CHECK_HV ();
  gnurcs_init (&program);

  labels = 0;

  for (; (a = *++argv) && *a++ == '-'; --argc)
    {
      switch (*a++)
        {
        case 'A':
        case 'E':
        case 'e':
          if (edarg && edarg[1] != (*argv)[1])
            PERR ("%s and %s are incompatible", edarg, *argv);
          edarg = *argv;
          break;

        case 'p':
          tostdout = true;
          break;
        case 'q':
          BE (quiet) = true;
          break;

        case 'L':
          if (3 <= labels)
            PFATAL ("too many -L options");
          if (!(LABEL (labels++) = *++argv))
            PFATAL ("-L needs following argument");
          --argc;
          break;

        case 'V':
          display_version (&program);
          gnurcs_goodbye ();
          return EXIT_SUCCESS;

        default:
          bad_option (a - 2);
          continue;
        }
      if (*a)
        bad_option (a - 2);
    }

  if (argc != 4)
    PFATAL ("%s arguments", argc < 4 ? "not enough" : "too many");

  for (int i = 0; i < 3; i++)
    {
      FNAME (i) = argv[i];
      if (labels <= i)
        LABEL (i) = FNAME (i);
    }

  if (FLOW (erroneousp))
    BOW_OUT ();
  exitstatus = merge (tostdout, edarg, three_manifestations);
  gnurcs_goodbye ();
  return exitstatus;
}

/*:help
[options] receiving-sibling parent other-sibling
Options:
  -A            Use `diff3 -A' style.
  -E            Use `diff3 -E' style (default).
  -e            Use `diff3 -e' style.
  -p            Write to stdout instead of overwriting RECEIVING-SIBLING.
  -q            Quiet mode; suppress conflict warnings.
  -LLABEL       (up to three times) Specify the conflict labels for
                RECEIVING-SIBLING, PARENT and OTHER-SIBLING, respectively.
  -V            Like --version.
*/

/* merge.c ends here */

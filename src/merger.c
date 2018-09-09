/* three-way file merge internals

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
#include <string.h>
#include <stdlib.h>
#include "b-complain.h"
#include "b-divvy.h"
#include "b-fb.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-merger.h"

static char const *
normalize_arg (char const *s)
/* If ‘s’ looks like an option, prepend ./ to it.  Return the result.  */
{
  if (*s == '-')
    {
      accf (PLEXUS, ".%c", SLASH);
      return str_save (s);
    }
  else
    return s;
}

int
merge (bool tostdout, char const *edarg, struct symdef three_manifestations[3])
/* Do ‘merge [-p] EDARG -L l0 -L l1 -L l2 a0 a1 a2’, where ‘tostdout’
   specifies whether ‘-p’ is present, ‘edarg’ gives the editing type
   (e.g. "-A", or null for the default), and lN and aN are taken from
   three_manifestations[N].{meaningful,underlying}, respectively.
   Return ‘DIFF_SUCCESS’ or ‘DIFF_FAILURE’.  */
{
  register int i;
  FILE *f;
  struct fro *rt;
  char const *a[3], *t;
  int s;
#if !DIFF3_BIN
  char const *d[2];
#endif

  for (i = 3; 0 <= --i;)
    a[i] = normalize_arg (FNAME (i));

  if (!edarg)
    edarg = "-E";

#if DIFF3_BIN
  t = NULL;
  if (!tostdout)
    t = maketemp (0);
  s = run (-1, t, prog_diff3, edarg, "-am",
           "-L", LABEL (0), "-L", LABEL (1), "-L", LABEL (2),
           a[0], a[1], a[2], NULL);
  if (DIFF_TROUBLE == s)
    BOW_OUT ();
  if (DIFF_FAILURE == s)
    PWARN ("conflicts during merge");
  if (t)
    {
      if (!(f = fopen_safer (FNAME (0), "w")))
        fatal_sys (FNAME (0));
      if (!(rt = fro_open (t, "r", NULL)))
        fatal_sys (t);
      fro_spew (rt, f);
      fro_close (rt);
      Ozclose (&f);
    }
#else  /* !DIFF3_BIN */
  for (i = 0; i < 2; i++)
    if (DIFF_TROUBLE == run (-1, d[i] = maketemp (i), prog_diff,
                             a[i], a[2], NULL))
      PFATAL ("diff failed");
  t = maketemp (2);
  s = run (-1, t,
           prog_diff3, edarg, d[0], d[1], a[0], a[1], a[2],
           LABEL (0), LABEL (2), NULL);
  if (s != DIFF_SUCCESS)
    {
      s = DIFF_FAILURE;
      PWARN ("overlaps or other problems during merge");
    }
  if (!(f = fopen_safer (t, "a+")))
    fatal_sys (t);
  aputs (tostdout ? "1,$p\n" : "w\n", f);
  rewind (f);
  aflush (f);
  if (run (fileno (f), NULL, ED, "-", a[0], NULL))
    BOW_OUT ();
  Ozclose (&f);
#endif  /* !DIFF3_BIN */

  tempunlink ();
  return s;
}

/* merger.c ends here */

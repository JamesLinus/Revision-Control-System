/* b-peer.c --- finding the ‘execv’able name of a peer program

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

#include "base.h"
#include <stdlib.h>
#include <string.h>
#include "findprog.h"
#include "b-complain.h"
#include "b-divvy.h"

struct symdef peer_super = { .meaningful = "grcs", .underlying = NULL };

const char *
one_beyond_last_dir_sep (const char *name)
{
  const char *end = strrchr (name, SLASH);

  return end
    ? 1 + end
    : NULL;
}

char const *
find_peer_prog (struct symdef *prog)
{
  if (! prog->underlying)
    {
      size_t len;

#ifndef EXEEXT

      /* Find the driver's invocation directory, once.  */
      if (! BE (invdir))
        {
          char const *name = find_in_path (PROGRAM (invoke));
          char const *end = one_beyond_last_dir_sep (name);

          if (!end)
            PFATAL ("cannot determine directory (in PATH) of `%s'", name);
          BE (invdir) = intern (PLEXUS, name, end - name);
          if (name != PROGRAM (invoke))
            free ((void *) name);
        }

      /* Concat the invocation directory with the base name.  */
      accf (PLEXUS, "%s%s", BE (invdir), prog->meaningful);

#else  /* EXEEXT */

      accf (PLEXUS, "%s" EXEEXT, prog->meaningful);

#endif /* EXEEXT */

      prog->underlying = finish_string (PLEXUS, &len);
    }

  return prog->underlying;
}

/* b-peer.c ends here */

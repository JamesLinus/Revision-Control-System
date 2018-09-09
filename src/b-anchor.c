/* b-anchor.c --- constant data and their lookup funcs

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
#include <string.h>

char const ks_revno[] = "revision number";

char const prog_diff[] = DIFF;
char const prog_diff3[] = DIFF3;
char const diff_flags[] = DIFFFLAGS;

char const equal_line[] =
  "=============================================================================\n";

#if TINY_INIT_NEEDS_EXPLICIT_NUL
#define EXPLICIT_NUL "\0"
#else
#define EXPLICIT_NUL
#endif

#define TINY_INIT(x)  { .len = sizeof (x) - 1, .bytes = x EXPLICIT_NUL }
#define TINYK(x)      TINY_DECL (x) = TINY_INIT (#x)

/* For libgnurcs.so, these should be coalesced into a pool.  */
TINY_DECL (ciklog) = TINY_INIT ("checked in with -k by ");
TINYK (access);
TINYK (author);
TINYK (branch);
TINYK (branches);
TINYK (comment);
TINYK (commitid);
TINYK (date);
TINYK (desc);
TINYK (expand);
TINYK (head);
TINYK (integrity);
TINYK (locks);
TINYK (log);
TINYK (next);
TINYK (state);
TINYK (strict);
#if COMPAT2
TINYK (suffix);
#endif
TINYK (symbols);
TINYK (text);

bool
looking_at (struct tinysym const *sym, char const *start)
{
  return MEM_SAME (sym->len, start, sym->bytes);
}

static const uint8_t kwsub_pool[22] =
{
  6 /* count */,
  2,'k','v','\0',
  3,'k','v','l','\0',
  1,'k','\0',
  1,'v','\0',
  1,'o','\0',
  1,'b','\0'
};

static const uint8_t keyword_pool[80] =
{
  11 /* count */,
  6,'A','u','t','h','o','r','\0',
  4,'D','a','t','e','\0',
  6,'H','e','a','d','e','r','\0',
  2,'I','d','\0',
  6,'L','o','c','k','e','r','\0',
  3,'L','o','g','\0',
  4,'N','a','m','e','\0',
  7,'R','C','S','f','i','l','e','\0',
  8,'R','e','v','i','s','i','o','n','\0',
  6,'S','o','u','r','c','e','\0',
  5,'S','t','a','t','e','\0'
};

static bool
pool_lookup (const uint8_t pool[], struct cbuf const *x,
             struct pool_found *found)
{
  const uint8_t *p = pool + 1;

  for (size_t i = 0; i < pool[0]; i++)
    {
      size_t symlen = *p;

      if (x->size == symlen && !memcmp (p + 1, x->string, symlen))
        {
          found->i = i;
          found->sym = (struct tinysym const *) p;
          return true;
        }
      p += 1 + symlen + 1;
    }
  return false;
}

int
recognize_kwsub (struct cbuf const *x)
/* Search for match in ‘kwsub_pool’ for byte range ‘x->string’ length ‘x->size’.
   Return its ‘enum kwsub’ if successful, otherwise -1.  */
{
  struct pool_found found;

  return pool_lookup (kwsub_pool, x, &found)
    ? found.i
    : -1;
}

int
str2expmode (char const *s)
/* Search for match in ‘kwsub_pool’ for string ‘s’.
   Return its ‘enum kwsub’ if successful, otherwise -1.  */
{
  const struct cbuf x =
    {
      .string = s,
      .size = strlen (s)
    };

  return recognize_kwsub (&x);
}

char const *
kwsub_string (enum kwsub i)
{
  size_t count = kwsub_pool[0], symlen;
  const uint8_t *p = kwsub_pool + 1;

  while (i && --count)
    {
      symlen = *p;
      p += 1 + symlen + 1;
      i--;
    }
  return i
    ? NULL
    : (char const *) (p + 1);
}

bool
recognize_keyword (char const *string, struct pool_found *found)
/* Check whether ‘string’ starts with a keyword followed by a
   ‘KDELIM’ or a ‘VDELIM’.  Return true if successful.  In that
   case, ‘found’ will hold a pointer to the found ‘struct tinysym’,
   as well as the associated ‘enum marker’ value.  */
{
  const char delims[3] = { KDELIM, VDELIM, '\0' };
  size_t limit = strcspn (string, delims);
  const struct cbuf x =
    {
      .string = string,
      .size = limit
    };

  return ((KDELIM == string[limit]
           || VDELIM == string[limit])
          && pool_lookup (keyword_pool, &x, found));
}

/* b-anchor.c ends here */

/* b-esds.c --- embarrassingly simple data structures

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
#include "b-divvy.h"
#include "b-esds.h"

#define STRUCTALLOC(to,type)  alloc (to, #type, sizeof (type))
#define NEWPAIR(to,sub)  STRUCTALLOC (to, struct sub)

#define EXTEND_BODY(sub)                        \
  struct sub *pair = NEWPAIR (to, sub);         \
                                                \
  pair->entry = x;                              \
  pair->next = NULL;                            \
  tp->next = pair;                              \
  return pair

struct link *
extend (struct link *tp, void const *x, struct divvy *to)
{
  EXTEND_BODY (link);
}

struct wlink *
wextend (struct wlink *tp, void *x, struct divvy *to)
{
  EXTEND_BODY (wlink);
}

#define PREPEND_BODY(sub)                       \
  struct sub *pair = NEWPAIR (to, sub);         \
                                                \
  pair->entry = x;                              \
  pair->next = ls;                              \
  return pair

struct link *
prepend (void const *x, struct link *ls, struct divvy *to)
{
  PREPEND_BODY (link);
}

struct wlink *
wprepend (void *x, struct wlink *ls, struct divvy *to)
{
  PREPEND_BODY (wlink);
}

/* b-esds.c ends here */

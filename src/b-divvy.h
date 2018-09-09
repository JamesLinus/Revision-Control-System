/* b-divvy.h --- dynamic memory manglement

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

struct divvy
{
  char const *name;
  struct obstack *space;
  void *first;
  size_t count;
};

extern struct divvy *plexus;
extern struct divvy *single;

extern void *okalloc (void *p);
extern struct divvy *make_space (char const name[]);
extern void *alloc (struct divvy *divvy, char const *what, size_t len);
extern void *zlloc (struct divvy *divvy, char const *what, size_t len);
extern char *intern (struct divvy *divvy, char const *s, size_t len);
extern void brush_off (struct divvy *divvy, void *ptr);
extern void forget (struct divvy *divvy);
extern void accf (struct divvy *divvy, char const *fmt, ...);
extern void accumulate_byte (struct divvy *divvy, int c);
extern void accumulate_range (struct divvy *divvy,
                              char const *beg, char const *end);
extern char *finish_string (struct divvy *divvy, size_t *result_len);
extern void *pointer_array (struct divvy *divvy, size_t count);
extern void close_space (struct divvy *divvy);

/* Idioms.  */

#define PLEXUS  plexus
#define SINGLE  single

#define ZLLOC(n,type)          (zlloc (PLEXUS, #type, sizeof (type) * n))
#define FALLOC(type)           (alloc (SINGLE, #type, sizeof (type)))
#define FZLLOC(type)           (zlloc (SINGLE, #type, sizeof (type)))
#define accs(divvy,string)     accf (divvy, "%s", string)

#define SHACCR(b,e)      accumulate_range (PLEXUS, b, e)
#define SHSTR(szp)       finish_string (PLEXUS, szp)
#define SHSNIP(szp,b,e)  (SHACCR (b, e), SHSTR (szp))

/* b-divvy.h ends here */

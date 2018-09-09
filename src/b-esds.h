/* b-esds.h --- embarrassingly simple data structures

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

struct link
{
  void const *entry;
  struct link *next;
};

struct wlink
{
  void *entry;
  struct wlink *next;
};

extern struct link *extend (struct link *tp, void const *x, struct divvy *to);
extern struct wlink *wextend (struct wlink *tp, void *x, struct divvy *to);
extern struct link *prepend (void const *x, struct link *ls, struct divvy *to);
extern struct wlink *wprepend (void *x, struct wlink *ls, struct divvy *to);

/* b-esds.h ends here */

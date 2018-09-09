/* Parse a string, returning a ‘struct partime’ that describes it.

   Copyright (C) 2010-2012 Thien-Thi Nguyen
   Copyright (C) 1993, 1994, 1995 Paul Eggert

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

#define TM_UNDEFINED   (-1)
#define TM_DEFINED(x)  (0 <= (x))

#define TM_UNDEFINED_ZONE  ((long) -24 * 60 * 60)
#define TM_LOCAL_ZONE      (TM_UNDEFINED_ZONE - 1)

struct partime
{
  /* This structure describes the parsed time.
     Only the following tm_* values in it are used:
     sec, min, hour, mday, mon, year, wday, yday.
     If ‘TM_UNDEFINED (value)’, the parser never found the value.
     The tm_year field is the actual year, not the year - 1900;
     but see ‘ymodulus’ below.  */
  struct tm tm;

  /* If ‘!TM_UNDEFINED (ymodulus)’, then
     ‘tm.tm_year’ is actually modulo ‘ymodulus’.  */
  int ymodulus;

  /* Week of year, ISO 8601 style.
     If ‘TM_UNDEFINED (yweek)’, the parser never found yweek.
     Weeks start on Mondays.  Week 1 includes Jan 4.  */
  int yweek;

  /* Seconds east of UTC; or ‘TM_LOCAL_ZONE’ or ‘TM_UNDEFINED_ZONE’.  */
  long zone;
};

char const *partime (char const *s, struct partime *t);
char const *parzone (char const *s, long *zone);

/* partime.h ends here */

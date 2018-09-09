/* Return ‘time_t’ from ‘struct partime’ returned by partime.

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

struct maketimestuff
{
  bool tzset_called_p;
  /* True if we have already called ‘tzset’.
     -- local_tm  */

  char const *TZ;
  /* The value of env var ‘TZ’.
     Only valid/used if ‘TZ_must_be_set’.
     -- time2tm  */

  struct tm time2tm_stash;
  /* Keep latest ‘time2tm’ value here.
     -- time2tm  */

  time_t t_cache[2];
  struct tm tm_cache[2];
  /* Cache the most recent ‘t’,‘tm’ pairs;
     [0] for UTC, [1] for local time.
     -- tm2time  */
};

struct tm *local_tm (const time_t *timep, struct tm *result);
struct tm *time2tm (time_t unixtime, bool localzone);
time_t difftm (struct tm const *a, struct tm const *b);
time_t str2time (char const *source, time_t default_time, long default_zone);
time_t tm2time (struct tm *tm, bool localzone, int yweek);
void adjzone (struct tm *t, long seconds);

/* maketime.h ends here */

/* b-isr.h --- interrupt service routine

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

void maybe_reset_sigchld (void);

extern void complain_signal (char const *msg, int signo);

struct isr_scratch;

enum isr_actions
  {
    ISR_CATCHINTS,
    ISR_IGNOREINTS,
    ISR_RESTOREINTS,
    ISR_CATCHMMAPINTS,
  };

extern struct isr_scratch *isr_init (bool *be_quiet);
extern void access_page (struct isr_scratch *scratch,
                         char const *filename,
                         char const *p);
extern void isr_do (struct isr_scratch *scratch,
                    enum isr_actions action);

/* Idioms.  */

#define ISR_DISABLE()   ISR_SCRATCH = NULL
#define ISR_DO(action)  isr_do (ISR_SCRATCH, ISR_ ## action)

#define IGNOREINTS()   ISR_DO (IGNOREINTS)
#define RESTOREINTS()  ISR_DO (RESTOREINTS)

#define ISR_SCRATCH  (BE (isr))

/* b-isr.h ends here */

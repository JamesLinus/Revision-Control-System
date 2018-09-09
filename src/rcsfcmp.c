/* Compare working files, ignoring RCS keyword strings.

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

#include "base.h"
#include <string.h>
#include "b-complain.h"
#include "b-fro.h"

static int
discardkeyval (int c, register struct fro *f)
{
  for (;;)
    switch (c)
      {
      case KDELIM:
      case '\n':
        return c;
      default:
        GETCHAR_OR (c, f, return EOF);
        break;
      }
}

int
rcsfcmp (register struct fro *xfp, struct stat const *xstatp,
         char const *uname, struct delta const *delta)
/* Compare the files ‘xfp’ and ‘uname’.  Return zero if ‘xfp’ has the
   same contents as ‘uname’ and neither has keywords, otherwise -1 if
   they are the same ignoring keyword values, and 1 if they differ even
   ignoring keyword values.  For the ‘Log’ keyword, skip the log message
   given by the parameter ‘delta’ in ‘xfp’.  Thus, return nonpositive if
   ‘xfp’ contains the same as ‘uname’, with the keywords expanded.

   Implementation: character-by-character comparison until $ is found.
   If a $ is found, read in the marker keywords; if they are real
   keywords and identical, read in keyword value. If value is terminated
   properly, disregard it and optionally skip log message; otherwise,
   compare value.  */
{
  int xc, uc;
  char xkeyword[keylength + 2];
  bool eqkeyvals;
  register struct fro *ufp;
  register bool xeof, ueof;
  register char *tp;
  register char const *sp;
  register size_t leaderlen;
  int result;
  struct pool_found match1;
  struct stat ustat;

  if (!(ufp = fro_open (uname, FOPEN_R_WORK, &ustat)))
    {
      fatal_sys (uname);
    }
  xeof = ueof = false;
  if (MIN_UNEXPAND <= BE (kws))
    {
      if (!(result = xstatp->st_size != ustat.st_size))
        {
          /* The fast path is possible only if neither file uses stdio.  */
          if (RM_STDIO != xfp->rm
              && RM_STDIO != ufp->rm)
            result = MEM_DIFF (xstatp->st_size, xfp->base, ufp->base);
          else
            for (;;)
              {
                /* Get the next characters.  */
                GETCHAR_OR (xc, xfp, xeof = true);
                GETCHAR_OR (uc, ufp, ueof = true);
                if (xeof | ueof)
                  goto eof;
                if (xc != uc)
                  goto return1;
              }
        }
    }
  else
    {
      xc = 0;
      uc = 0;                   /* Keep lint happy.  */
      leaderlen = 0;
      result = 0;

      for (;;)
        {
          if (xc != KDELIM)
            {
              /* Get the next characters.  */
              GETCHAR_OR (xc, xfp, xeof = true);
              GETCHAR_OR (uc, ufp, ueof = true);
              if (xeof | ueof)
                goto eof;
            }
          else
            {
              /* Try to get both keywords.  */
              tp = xkeyword;
              for (;;)
                {
                  GETCHAR_OR (xc, xfp, xeof = true);
                  GETCHAR_OR (uc, ufp, ueof = true);
                  if (xeof | ueof)
                    goto eof;
                  if (xc != uc)
                    break;
                  switch (xc)
                    {
                    default:
                      if (xkeyword + keylength <= tp)
                        break;
                      *tp++ = xc;
                      continue;
                    case '\n':
                    case KDELIM:
                    case VDELIM:
                      break;
                    }
                  break;
                }
              if ((xc == KDELIM || xc == VDELIM)
                  && (uc == KDELIM || uc == VDELIM)
                  && (*tp = xc, recognize_keyword (xkeyword, &match1)))
                {
#ifdef FCMPTEST
                  printf ("found common keyword %s\n", xkeyword);
#endif
                  result = -1;
                  for (;;)
                    {
                      if (xc != uc)
                        {
                          xc = discardkeyval (xc, xfp);
                          uc = discardkeyval (uc, ufp);
                          if ((xeof = xc == EOF) | (ueof = uc == EOF))
                            goto eof;
                          eqkeyvals = false;
                          break;
                        }
                      switch (xc)
                        {
                        default:
                          GETCHAR_OR (xc, xfp, xeof = true);
                          GETCHAR_OR (uc, ufp, ueof = true);
                          if (xeof | ueof)
                            goto eof;
                          continue;

                        case '\n':
                        case KDELIM:
                          eqkeyvals = true;
                          break;
                        }
                      break;
                    }
                  if (xc != uc)
                    goto return1;
                  if (xc == KDELIM)
                    {
                      /* Skip closing KDELIM.  */
                      GETCHAR_OR (xc, xfp, xeof = true);
                      GETCHAR_OR (uc, ufp, ueof = true);
                      if (xeof | ueof)
                        goto eof;
                      /* If the keyword is ‘Log’, also
                         skip the log message in ‘xfp’.  */
                      if (match1.i == Log)
                        {
                          /* First, compute the number of LFs in log msg.  */
                          int lncnt;
                          size_t ls, ccnt;

                          sp = delta->pretty_log.string;
                          ls = delta->pretty_log.size;
                          if (!looking_at (&TINY (ciklog), delta->pretty_log.string))
                            {
                              /* This log message was inserted.  Skip
                                 its header.  The number of newlines to
                                 skip is ‘1 + (C + 1) * (1 + L + 1)’,
                                 where C is the number of newlines in
                                 the comment leader, and L is the number
                                 of newlines in the log string.  */
                              int c1 = 1;

                              for (ccnt = REPO (log_lead).size; ccnt--;)
                                c1 += REPO (log_lead).string[ccnt] == '\n';
                              lncnt = 2 * c1 + 1;
                              while (ls--)
                                if (*sp++ == '\n')
                                  lncnt += c1;
                              for (;;)
                                {
                                  if (xc == '\n')
                                    if (--lncnt == 0)
                                      break;
                                  GETCHAR_OR (xc, xfp, goto returnresult);
                                }
                              /* Skip last comment leader.  Can't just
                                 skip another line here, because there
                                 may be additional characters on the
                                 line (after the Log....$).  */
                              ccnt = BE (version) < VERSION (5)
                                ? REPO (log_lead).size
                                : leaderlen;
                              do
                                {
                                  GETCHAR_OR (xc, xfp, goto returnresult);
                                  /* Read to the end of the comment leader
                                     or '\n', whatever comes first, because
                                     the leader's trailing white space was
                                     probably stripped.  */
                                }
                              while (ccnt-- && (xc != '\n' || --c1));
                            }
                        }
                    }
                  else
                    {
                      /* Both end in the same character, but not a ‘KDELIM’.
                         Must compare string values.  */
#ifdef FCMPTEST
                      printf
                        ("non-terminated keywords %s, potentially different values\n",
                         xkeyword);
#endif
                      if (!eqkeyvals)
                        goto return1;
                    }
                }
            }
          if (xc != uc)
            goto return1;
          if (xc == '\n')
            leaderlen = 0;
          else
            leaderlen++;
        }
    }

eof:
  if (xeof == ueof)
    goto returnresult;
return1:
  result = 1;
returnresult:
  fro_close (ufp);
  return result;
}

/* rcsfcmp.c ends here */

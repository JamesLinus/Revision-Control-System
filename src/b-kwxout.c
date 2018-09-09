/* b-kwxout.c --- keyword expansion on output

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
#include "b-divvy.h"
#include "b-fb.h"
#include "b-fro.h"
#include "b-kwxout.h"

static void
escape_string (register FILE *out, register char const *s)
/* Output to ‘out’ the string ‘s’,
   escaping chars that would break ‘ci -k’.  */
{
  register char c;

  for (;;)
    switch ((c = *s++))
      {
      case 0:
        return;
      case '\t':
        aputs ("\\t", out);
        break;
      case '\n':
        aputs ("\\n", out);
        break;
      case ' ':
        aputs ("\\040", out);
        break;
      case KDELIM:
        aputs ("\\044", out);
        break;
      case '\\':
        if (VERSION (5) <= BE (version))
          {
            aputs ("\\\\", out);
            break;
          }
        /* fall into */
      default:
        aputc (c, out);
        break;
      }
}

static void
keyreplace (struct pool_found *marker, struct expctx *ctx)
/* Output the keyword value(s) corresponding to ‘marker’.
   Attributes are derived from ‘delta’.  */
{
  struct fro *infile = ctx->from;
  register FILE *out = ctx->to;
  register struct delta const *delta = ctx->delta;
  bool dolog = ctx->dolog, delimstuffed = ctx->delimstuffed;
  register char const *sp, *cp, *date;
  int c;
  register size_t cs, cw, ls;
  char const *sp1;
  char datebuf[datesize + zonelenmax];
  int RCSv;
  int exp;
  bool include_locker = BE (inclusive_of_Locker_in_Id_val);

  exp = BE (kws);
  date = delta->date;
  RCSv = BE (version);

  if (exp != kwsub_v)
    aprintf (out, "%c%s", KDELIM, marker->sym->bytes);
  if (exp != kwsub_k)
    {
      if (exp != kwsub_v)
        aprintf (out, "%c%c", VDELIM,
                 marker->i == Log && RCSv < VERSION (5) ? '\t' : ' ');

      switch (marker->i)
        {
        case Author:
          aputs (delta->author, out);
          break;
        case Date:
          aputs (date2str (date, datebuf), out);
          break;
        case Id:
        case Header:
          escape_string (out,
                         marker->i == Id || RCSv < VERSION (4)
                         ? basefilename (REPO (filename)) : getfullRCSname ());
          aprintf (out, " %s %s %s %s",
                   delta->num,
                   date2str (date, datebuf),
                   delta->author,
                   RCSv == VERSION (3) && delta->lockedby ? "Locked"
                   : delta->state);
          if (delta->lockedby)
            {
              if (VERSION (5) <= RCSv)
                {
                  if (include_locker || exp == kwsub_kvl)
                    aprintf (out, " %s", delta->lockedby);
                }
              else if (RCSv == VERSION (4))
                aprintf (out, " Locker: %s", delta->lockedby);
            }
          break;
        case Locker:
          if (delta->lockedby)
            if (include_locker || exp == kwsub_kvl || RCSv <= VERSION (4))
              aputs (delta->lockedby, out);
          break;
        case Log:
        case RCSfile:
          escape_string (out, basefilename (REPO (filename)));
          break;
        case Name:
          if (delta->name)
            aputs (delta->name, out);
          break;
        case Revision:
          aputs (delta->num, out);
          break;
        case Source:
          escape_string (out, getfullRCSname ());
          break;
        case State:
          aputs (delta->state, out);
          break;
        default:
          break;
        }
      if (exp != kwsub_v)
        afputc (' ', out);
    }
  if (exp != kwsub_v)
    afputc (KDELIM, out);

  if (marker->i == Log && dolog)
    {
      char *leader = NULL;

      sp = delta->pretty_log.string;
      ls = delta->pretty_log.size;
      if (looking_at (&TINY (ciklog), delta->pretty_log.string))
        return;
      if (RCSv < VERSION (5))
        {
          cp = REPO (log_lead).string;
          cs = REPO (log_lead).size;
        }
      else
        {
          bool kdelim_found = false;
          off_t chars_read = fro_tello (infile);

          c = 0;                /* Pacify ‘gcc -Wall’.  */

          /* Back up to the start of the current input line,
             setting ‘cs’ to the number of characters before ‘$Log’.  */
          cs = 0;
          for (;;)
            {
#define GET_PREV_BYTE()  do                     \
                {                               \
                  fro_move (infile, -2);        \
                  GETCHAR (c, infile);          \
                }                               \
              while (0)

              if (!--chars_read)
                goto done_backing_up;
              GET_PREV_BYTE ();
              if (c == '\n')
                break;
              if (c == SDELIM && delimstuffed)
                {
                  if (!--chars_read)
                    break;
                  GET_PREV_BYTE ();
                  if (c != SDELIM)
                    {
                      GETCHAR (c, infile);
                      break;
                    }
                }
              cs += kdelim_found;
              kdelim_found |= c == KDELIM;
#undef GET_PREV_BYTE
            }
          GETCHAR (c, infile);
        done_backing_up:
          ;

          /* Copy characters before ‘$Log’ into ‘leader’.  */
          leader = alloc (SINGLE, "leader", 1 + cs);
          cp = leader;
          for (cw = 0; cw < cs; cw++)
            {
              leader[cw] = c;
              if (c == SDELIM && delimstuffed)
                GETCHAR (c, infile);
              GETCHAR (c, infile);
            }

          /* Convert traditional C or Pascal leader to " *".  */
          for (cw = 0; cw < cs; cw++)
            if (ctab[(unsigned char) cp[cw]] != SPACE)
              break;
          if (cw + 1 < cs
              && cp[cw + 1] == '*' && (cp[cw] == '/' || cp[cw] == '('))
            {
              size_t i = cw + 1;

              for (;;)
                if (++i == cs)
                  {
                    PWARN ("`%c* $Log' is obsolescent; use ` * $Log'.", cp[cw]);
                    leader[cw] = ' ';
                    break;
                  }
                else if (ctab[(unsigned char) cp[i]] != SPACE)
                  break;
            }

          /* Skip ‘$Log ... $’ string.  */
          do
            GETCHAR (c, infile);
          while (c != KDELIM);
        }
      afputc ('\n', out);
      awrite (cp, cs, out);
      sp1 = date2str (date, datebuf);
      if (VERSION (5) <= RCSv)
        {
          aprintf (out, "Revision %s  %s  %s",
                   delta->num, sp1, delta->author);
        }
      else
        {
          /* Oddity: 2 spaces between date and time, not 1 as usual.  */
          sp1 = strchr (sp1, ' ');
          aprintf (out, "Revision %s  %.*s %s  %s",
                   delta->num, (int) (sp1 - datebuf), datebuf,
                   sp1, delta->author);
        }
      /* Do not include state: it may change and is not updated.  */
      cw = cs;
      if (VERSION (5) <= RCSv)
        for (; cw && (cp[cw - 1] == ' ' || cp[cw - 1] == '\t'); --cw)
          continue;
      for (;;)
        {
          afputc ('\n', out);
          awrite (cp, cw, out);
          if (!ls)
            break;
          --ls;
          c = *sp++;
          if (c != '\n')
            {
              awrite (cp + cw, cs - cw, out);
              do
                {
                  afputc (c, out);
                  if (!ls)
                    break;
                  --ls;
                  c = *sp++;
                }
              while (c != '\n');
            }
        }
      if (leader)
        brush_off (SINGLE, leader);
    }
}

int
expandline (struct expctx *ctx)
/* Read a line from ‘ctx->from’ and write it to ‘ctx->to’.  Do keyword
   expansion with data from ‘ctx->delta’.  If ‘ctx->delimstuffed’ is true,
   double ‘SDELIM’ is replaced with single ‘SDELIM’.  If ‘ctx->rewr’ is
   set, copy the line unchanged to ‘ctx->rewr’.  ‘ctx->delimstuffed’ must
   be true if ‘ctx->rewr’ is set.  Append revision history to log only if
   ‘ctx->dolog’ is set.  Return -1 if no data is copied, 0 if an
   incomplete line is copied, 2 if a complete line is copied; add 1 to
   return value if expansion occurred.  */
{
  struct divvy *lparts = ctx->lparts;
  struct fro *fin = ctx->from;
  bool delimstuffed = ctx->delimstuffed;
  int c;
  register FILE *out, *frew;
  register int r;
  bool e;
  struct pool_found matchresult;
  char *cooked = NULL;
  size_t len;

  if (!lparts)
    lparts = ctx->lparts = make_space ("lparts");

  out = ctx->to;
  frew = ctx->rewr;
  forget (lparts);
  e = false;
  r = -1;

  for (;;)
    {
#define GETCHAR_ELSE_GOTO(label)  GETCHAR_OR (c, fin, goto label);
      if (delimstuffed)
        TEECHAR ();
      else
        GETCHAR_ELSE_GOTO (done);
      for (;;)
        {
          switch (c)
            {
            case SDELIM:
              if (delimstuffed)
                {
                  TEECHAR ();
                  if (c != SDELIM)
                    /* End of string.  */
                    goto done;
                }
              /* fall into */
            default:
              aputc (c, out);
              r = 0;
              break;

            case '\n':
              aputc (c, out);
              r = 2;
              goto done;

            case KDELIM:
              r = 0;
              /* Check for keyword.  */
              accumulate_byte (lparts, KDELIM);
              len = 0;
              for (;;)
                {
                  if (delimstuffed)
                    TEECHAR ();
                  else
                    GETCHAR_ELSE_GOTO (keystring_eof);
                  if (len <= keylength + 3)
                    switch (ctab[c])
                      {
                      case LETTER:
                      case Letter:
                        accumulate_byte (lparts, c);
                        len++;
                        continue;
                      default:
                        break;
                      }
                  break;
                }
              accumulate_byte (lparts, c);
              cooked = finish_string (lparts, &len);
              if (! recognize_keyword (cooked + 1, &matchresult))
                {
                  cooked[len - 1] = '\0';
                  aputs (cooked, out);
                  /* Last c handled properly.  */
                  continue;
                }
              /* Now we have a keyword terminated with a K/VDELIM.  */
              if (c == VDELIM)
                {
                  /* Try to find closing ‘KDELIM’, and replace value.  */
                  for (;;)
                    {
                      if (delimstuffed)
                        TEECHAR ();
                      else
                        GETCHAR_ELSE_GOTO (keystring_eof);
                      if (c == '\n' || c == KDELIM)
                        break;
                      accumulate_byte (lparts, c);
                      if (c == SDELIM && delimstuffed)
                        {
                          /* Skip next ‘SDELIM’.  */
                          TEECHAR ();
                          if (c != SDELIM)
                            /* End of string before closing
                               ‘KDELIM’ or newline.  */
                            goto keystring_eof;
                        }
                    }
                  if (c != KDELIM)
                    {
                      /* Couldn't find closing ‘KDELIM’ -- give up.  */
                      cooked = finish_string (lparts, &len);
                      aputs (cooked, out);
                      /* Last c handled properly.  */
                      continue;
                    }
                  /* Ignore the region between VDELIM and KDELIM.  */
                  cooked = finish_string (lparts, &len);
                }
              /* Now put out the new keyword value.  */
              keyreplace (&matchresult, ctx);
              e = true;
              break;
            }
          break;
        }
#undef GETCHAR_ELSE_GOTO
    }

keystring_eof:
  cooked = finish_string (lparts, &len);
  aputs (cooked, out);
done:
  return r + e;
}

/* b-kwxout.c ends here */

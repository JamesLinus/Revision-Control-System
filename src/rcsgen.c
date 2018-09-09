/* Generate RCS revisions.

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
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include "b-complain.h"
#include "b-divvy.h"
#include "b-esds.h"
#include "b-fb.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-kwxout.h"

enum stringwork
{ enter, copy, edit, expand, edit_expand };

static void
scandeltatext (struct editstuff *es, struct wlink **ls,
               struct delta *delta, enum stringwork func, bool needlog)
/* Scan delta text nodes up to and including the one given by ‘delta’.
   For the one given by ‘delta’, the log message is saved into
   ‘delta->log’ if ‘needlog’ is set; ‘func’ specifies how to handle the
   text.  Does not advance input after finished.  */
{
  struct delta const *nextdelta;
  struct fro *from = FLOW (from);
  FILE *to = FLOW (to);
  struct atat *log, *text;
  struct range range;

  for (;; *ls = (*ls)->next)
    {
      nextdelta = (*ls)->entry;
      log = nextdelta->log;
      text = nextdelta->text;
      range.beg = nextdelta->neck;
      range.end = text->beg;
      if (needlog && delta == nextdelta)
        {
          /* TODO: Make ‘needlog’ a ‘struct cbuf *’ and stash there.  */
          delta->pretty_log = string_from_atat (SINGLE, log);
          delta->pretty_log = cleanlogmsg (delta->pretty_log.string,
                                           delta->pretty_log.size);
        }
      /* Skip over it.  */
      if (to)
        fro_spew_partial (to, from, &range);
      if (delta == nextdelta)
        break;
      /* Skip over it.  */
      if (to)
        atat_put (to, text);
    }
  fro_move (from, range.end);
  switch (func)
    {
    case enter:
      enterstring (es, text);
      break;
    case copy:
      copystring (es, text);
      break;
    case expand:
      /* Read a string terminated by ‘SDELIM’ from ‘FLOW (from)’ and
         write it to ‘FLOW (res)’.  Double ‘SDELIM’ is replaced with
         single ‘SDELIM’.  Keyword expansion is performed with data
         from ‘delta’.  If ‘FLOW (to)’ is non-NULL, the string is
         also copied unchanged to ‘FLOW (to)’.  */
      {
        int c;
        struct expctx ctx = EXPCTX (FLOW (res), to, from, true, true);

        GETCHAR (c, from);
        if (to)
          afputc (c, to);
        while (1 < expandline (&ctx))
          continue;
        FINISH_EXPCTX (&ctx);
      }
      break;
    case edit:
      editstring (es, text, NULL);
      break;
    case edit_expand:
      editstring (es, text, delta);
      break;
    }
}

char const *
buildrevision (struct wlink const *deltas, struct delta *target,
               FILE *outfile, bool expandflag)
/* Generate the revision given by ‘target’ by retrieving all deltas given
   by parameter ‘deltas’ and combining them.  If ‘outfile’ is set, the
   revision is output to it, otherwise write into a temporary file.
   Temporary files are allocated by ‘maketemp’.  If ‘expandflag’ is set,
   keyword expansion is performed.  Return NULL if ‘outfile’ is set, the
   name of the temporary file otherwise.

   Algorithm: Copy initial revision unchanged.  Then edit all revisions
   but the last one into it, alternating input and output files
   (‘FLOW (result)’ and ‘editname’).  The last revision is then edited in,
   performing simultaneous keyword substitution (this saves one extra
   pass).  All this simplifies if only one revision needs to be generated,
   or no keyword expansion is necessary, or if output goes to stdout.  */
{
  struct editstuff *es = make_editstuff ();
  struct wlink *ls = GROK (deltas);

  if (deltas->entry == target)
    {
      /* Only latest revision to generate.  */
      openfcopy (outfile);
      scandeltatext (es, &ls, target, expandflag ? expand : copy, true);
    }
  else
    {
      /* Several revisions to generate.
         Get initial revision without keyword expansion.  */
      scandeltatext (es, &ls, deltas->entry, enter, false);
      while (ls = ls->next,
             (deltas = deltas->next)->next)
        {
          /* Do all deltas except last one.  */
          scandeltatext (es, &ls, deltas->entry, edit, false);
        }
      if (expandflag || outfile)
        {
          /* First, get to beginning of file.  */
          finishedit (es, NULL, outfile, false);
        }
      scandeltatext (es, &ls, target, expandflag ? edit_expand : edit, true);
      finishedit (es, expandflag ? target : NULL, outfile, true);
    }
  unmake_editstuff (es);
  if (outfile)
    return NULL;
  Ozclose (&FLOW (res));
  return FLOW (result);
}

struct cbuf
cleanlogmsg (char const *m, size_t s)
{
  struct cbuf r;

#define WHITESPACEP(c)  (' ' == c || '\t' == c || '\n' == c)
  while (s && WHITESPACEP (*m))
    s--, m++;
  while (s && WHITESPACEP (m[s - 1]))
    s--;
#undef WHITESPACEP

  r.string = m;
  r.size = s;
  return r;
}

bool
ttystdin (void)
{
  if (!BE (interactive_valid))
    {
      if (!BE (interactive))
        BE (interactive) = isatty (STDIN_FILENO);
      BE (interactive_valid) = true;
    }
  return BE (interactive);
}

int
getcstdin (void)
{
  register FILE *in;
  register int c;

  in = stdin;
  if (feof (in) && ttystdin ())
    clearerr (in);
  c = getc (in);
  if (c == EOF)
    {
      testIerror (in);
      if (feof (in) && ttystdin ())
        complain ("\n");
    }
  return c;
}

bool
yesorno (bool default_answer, char const *question, ...)
{
  va_list args;
  register int c, r;

  if (!BE (quiet) && ttystdin ())
    {
      oflush ();
      va_start (args, question);
      vcomplain (question, args);
      va_end (args);
      r = c = getcstdin ();
      while (c != '\n' && !feof (stdin))
        c = getcstdin ();
      if (r == 'y' || r == 'Y')
        return true;
      if (r == 'n' || r == 'N')
        return false;
    }
  return default_answer;
}

void
write_desc_maybe (FILE *to)
{
  struct atat *desc = GROK (desc);

  if (to)
    atat_put (to, desc);
}

void
putdesc (struct cbuf *cb, bool textflag, char *textfile)
/* Put the descriptive text into file ‘FLOW (rewr)’.
   Also, save the description text into ‘cb’.
   If ‘FLOW (from) && !textflag’, the text is copied from the old description.
   Otherwise, if ‘textfile’, the text is read from that file, or from
   stdin, if ‘!textfile’.  A ‘textfile’ with a leading '-' is treated as a
   string, not a filename.  If ‘FLOW (from)’, the old descriptive text is
   discarded.  Always clear ‘FLOW (to)’.  */
{
  register FILE *txt;
  register int c;
  register FILE *frew;
  register char *p;
  size_t s;
  struct fro *from = FLOW (from);

  frew = FLOW (rewr);
  if (from && !textflag)
    {
      /* Copy old description.  */
      aprintf (frew, "\n\n%s\n", TINYKS (desc));
      write_desc_maybe (frew);
    }
  else
    {
      FLOW (to) = NULL;
      /* Get new description.  */
      aprintf (frew, "\n\n%s\n%c", TINYKS (desc), SDELIM);
      if (!textfile)
        *cb = getsstdin ("t-", "description",
                         "NOTE: This is NOT the log message!\n");
      else if (!cb->string)
        {
          if (*textfile == '-')
            {
              p = textfile + 1;
              s = strlen (p);
            }
          else
            {
              if (!(txt = fopen_safer (textfile, "r")))
                fatal_sys (textfile);
              for (;;)
                {
                  if ((c = getc (txt)) == EOF)
                    {
                      testIerror (txt);
                      if (feof (txt))
                        break;
                    }
                  accumulate_byte (PLEXUS, c);
                }
              if (PROB (fclose (txt)))
                Ierror ();
              p = finish_string (PLEXUS, &s);
            }
          *cb = cleanlogmsg (p, s);
        }
      putstring (frew, false, *cb, true);
      aputc ('\n', frew);
    }
}

struct cbuf
getsstdin (char const *option, char const *name, char const *note)
{
  register int c;
  register char *p;
  register bool tty = ttystdin ();
  size_t len, column = 0;
  bool dot_in_first_column_p = false, discardp = false;

#define prompt  complain
  if (tty)
    prompt ("enter %s, terminated with single '.' or end of file:\n%s>> ",
            name, note);
  else if (feof (stdin))
    RFATAL ("can't reread redirected stdin for %s; use -%s<%s>",
            name, option, name);

  while (c = getcstdin (), !feof (stdin))
    {
      if (!column)
        dot_in_first_column_p = ('.' == c);
      if (c == '\n')
        {
          if (1 == column && dot_in_first_column_p)
            {
              discardp = true;
              break;
            }
          else if (tty)
            prompt (">> ");
          column = 0;
        }
      else
        column++;
      accumulate_byte (PLEXUS, c);
    }
  p = finish_string (PLEXUS, &len);
#undef prompt
  return cleanlogmsg (p, len - (discardp ? 1 : 0));
}

void
format_assocs (FILE *out, char const *fmt)
{
  for (struct link *ls = GROK (symbols); ls; ls = ls->next)
    {
      struct symdef const *d = ls->entry;

      aprintf (out, fmt, d->meaningful, d->underlying);
    }
}

void
format_locks (FILE *out, char const *fmt)
{
  for (struct link *ls = GROK (locks); ls; ls = ls->next)
    {
      struct rcslock const *rl = ls->entry;

      aprintf (out, fmt, rl->login, rl->delta->num);
    }
}

static char const *semi_lf = ";\n";
#define SEMI_LF()  aprintf (fout, "%s", semi_lf)

void
putadmin (void)
/* Output the admin node.  */
{
  register FILE *fout;
  struct repo *r = REPO (r);
  struct delta *tip = REPO (tip);
  char const *defbr = r ? GROK (branch) : NULL;
  int kws = BE (kws);

  if (!(fout = FLOW (rewr)))
    {
#if BAD_CREAT0
      ORCSclose ();
      fout = fopen_safer (makedirtemp (false), FOPEN_WB);
#else  /* !BAD_CREAT0 */
      int fo = REPO (fd_lock);

      REPO (fd_lock) = -1;
      fout = fdopen (fo, FOPEN_WB);
#endif  /* !BAD_CREAT0 */

      if (!(FLOW (rewr) = fout))
        fatal_sys (REPO (filename));
    }

  aprintf (fout, "%s\t%s%s", TINYKS (head), tip ? tip->num : "", semi_lf);
  if (defbr && VERSION (4) <= BE (version))
    aprintf (fout, "%s\t%s%s", TINYKS (branch), defbr, semi_lf);
  aputs (TINYKS (access), fout);
  for (struct link *ls = r ? GROK (access) : NULL; ls; ls = ls->next)
    aprintf (fout, "\n\t%s", (char const *) ls->entry);
  SEMI_LF ();
  aprintf (fout, "%s", TINYKS (symbols));
  format_assocs (fout, "\n\t%s:%s"); SEMI_LF ();
  aprintf (fout, "%s", TINYKS (locks));
  if (r)
    format_locks (fout, "\n\t%s:%s");
  if (BE (strictly_locking))
    aprintf (fout, "; %s", TINYKS (strict));
  SEMI_LF ();
  if (GROK (integrity))
    {
      aprintf (fout, "%s\n", TINYKS (integrity));
      atat_put (fout, GROK (integrity)); SEMI_LF ();
    }
  if (REPO (log_lead).size)
    {
      aprintf (fout, "%s\t", TINYKS (comment));
      putstring (fout, true, REPO (log_lead), false); SEMI_LF ();
    }
  if (kws != kwsub_kv)
    aprintf (fout, "%s\t%c%s%c%s",
             TINYKS (expand), SDELIM, kwsub_string (kws),
             SDELIM, semi_lf);
  aprintf (fout, "\n");
}

static void
putdelta (register struct delta const *node, register FILE *fout)
/* Output the delta ‘node’ to ‘fout’.  */
{
  if (!node)
    return;

  aprintf (fout, "\n%s\n%s\t%s;\t%s %s;\t%s %s%s%s",
           node->num, TINYKS (date), node->date, TINYKS (author), node->author,
           TINYKS (state), node->state ? node->state : "",
           semi_lf, TINYKS (branches));
  for (struct wlink *ls = node->branches; ls; ls = ls->next)
    {
      struct delta *delta = ls->entry;

      aprintf (fout, "\n\t%s", delta->num);
    }
  SEMI_LF ();

  aprintf (fout, "%s\t%s", TINYKS (next), node->ilk ? node->ilk->num : "");
  SEMI_LF ();
  if (node->commitid)
    aprintf (fout, "%s\t%s%s", TINYKS (commitid), node->commitid, semi_lf);
}

void
puttree (struct delta const *root, register FILE *fout)
/* Output the delta tree with base ‘root’ in preorder to ‘fout’.  */
{
  if (!root)
    return;

  if (root->selector)
    putdelta (root, fout);

  puttree (root->ilk, fout);

  for (struct wlink *ls = root->branches; ls; ls = ls->next)
    puttree (ls->entry, fout);
}

bool
putdtext (struct delta const *delta, char const *srcname,
          FILE *fout, bool diffmt)
/* Output a deltatext node with delta number ‘delta->num’, log message
   ‘delta->pretty_log’, and text ‘srcname’ to ‘fout’.  Double up all ‘SDELIM’s
   in both the log and the text.  Make sure the log message ends in '\n'.
   Return false on error.  If ‘diffmt’, also check that the text is valid
   "diff -n" output.  */
{
  struct fro *fin;

  if (!(fin = fro_open (srcname, "r", NULL)))
    {
      syserror_errno (srcname);
      return false;
    }
  putdftext (delta, fin, fout, diffmt);
  fro_close (fin);
  return true;
}

void
putstring (register FILE *out, bool delim, struct cbuf s, bool log)
/* Output to ‘out’ one ‘SDELIM’ if ‘delim’, then the string ‘s’ with
   ‘SDELIM’s doubled.  If ‘log’ is set then ‘s’ is a log string; append
   a newline if ‘s’ is nonempty.  */
{
  register char const *sp;
  register size_t ss;

  if (delim)
    aputc (SDELIM, out);
  sp = s.string;
  for (ss = s.size; ss; --ss)
    {
      if (*sp == SDELIM)
        aputc (SDELIM, out);
      aputc (*sp++, out);
    }
  if (s.size && log)
    aputc ('\n', out);
  aputc (SDELIM, out);
}

void
putdftext (struct delta const *delta, struct fro *finfile,
           FILE *foutfile, bool diffmt)
/* Like ‘putdtext’, except the source file is already open.  */
{
  register FILE *fout;
  int c;
  register struct fro *fin;
  int ed;
  struct diffcmd dc;

  fout = foutfile;
  aprintf (fout, "\n\n%s\n%s\n", delta->num, TINYKS (log));

  /* Put log.  */
  putstring (fout, true, delta->pretty_log, true);
  aputc ('\n', fout);
  /* Put text.  */
  aprintf (fout, "%s\n%c", TINYKS (text), SDELIM);

  fin = finfile;
  if (!diffmt)
    {
      /* Copy the file.  */
      for (;;)
        {
          GETCHAR_OR (c, fin, goto done);
          if (c == SDELIM)
            /* Double up ‘SDELIM’.  */
            aputc (SDELIM, fout);
          aputc (c, fout);
        }
    done:
      ;
    }
  else
    {
      initdiffcmd (&dc);
      while (0 <= (ed = getdiffcmd (fin, false, fout, &dc)))
        if (ed)
          {
            while (dc.nlines--)
              do
                {
                  GETCHAR_OR (c, fin,
                              {
                                if (!dc.nlines)
                                  goto OK_EOF;
                                unexpected_EOF ();
                              });
                  if (c == SDELIM)
                    aputc (SDELIM, fout);
                  aputc (c, fout);
                }
              while (c != '\n');
          }
    }
 OK_EOF:
  aprintf (fout, "%c\n", SDELIM);
}

/* rcsgen.c ends here */

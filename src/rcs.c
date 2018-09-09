/* Change RCS file attributes.

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
#include <stdlib.h>
#include "rcs.help"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-esds.h"
#include "b-excwho.h"
#include "b-fb.h"
#include "b-feph.h"
#include "b-fro.h"

struct u_log
{
  char const *revno;
  struct cbuf message;
};

struct u_state
{
  char const *revno;
  char const *status;
};

enum changeaccess
{ append, erase };
struct chaccess
{
  char const *login;
  enum changeaccess command;
};

struct delrevpair
{
  char const *strt;
  char const *end;
  int code;
};

struct adminstuff
{
  int rv;
  struct wlink *deltas;
  bool suppress_mail;
  bool lockhead;
  bool unlockcaller;
  struct link *newlocks;
  struct link *byelocks;

  /* For ‘-sSTATE’ handling.  */
  char const *headstate;
  bool headstate_changed_p;
  struct link states;
  struct link *tp_state;

  /* For ‘-a’, ‘-A’, ‘-e’ handling.  */
  struct link accesses;
  struct link *tp_access;

  /* For ‘-n’, ‘-N’ handling.  */
  struct link assocs;
  struct link *tp_assoc;

  /* For ‘-m’ handling.  */
  struct link logs;
  struct link *tp_log;

  /* For ‘-o’ handling.  */
  struct delta *cuthead, *cuttail, *delstrt;
  struct delrevpair delrev;
};

static void
cleanup (int *exitstatus)
{
  if (FLOW (erroneousp))
    *exitstatus = EXIT_FAILURE;
  fro_zclose (&FLOW (from));
  Ozclose (&FLOW (res));
  ORCSclose ();
  dirtempunlink ();
}

static void
getassoclst (struct adminstuff *dc, char *sp)
/* Associate a symbolic name to a revision or branch,
   and store in ‘dc->assocs’.  */
{
  char option = *sp++;
  struct u_symdef *ud;
  char const *name;
  size_t len;
  int c = *sp;
  struct link **tp = &dc->tp_assoc;

  if (! *tp)
    *tp = &dc->assocs;

#define SKIPWS()  while (c == ' ' || c == '\t' || c == '\n') c = *++sp

  SKIPWS ();
  /* Check for invalid symbolic name.  */
  name = SHSNIP (&len, sp, checksym (sp, ':'));
  sp += len;
  c = *sp;
  SKIPWS ();

  if (c != ':' && c != '\0')
    {
      PERR ("invalid string `%s' after option `-%c'", sp, option);
      return;
    }

  ud = ZLLOC (1, struct u_symdef);
  ud->u.meaningful = name;
  ud->override = ('N' == option);
  if (c == '\0')
    /* Delete symbol.  */
    ud->u.underlying = NULL;
  else
    /* Add association.  */
    {
      c = *++sp;
      SKIPWS ();
      ud->u.underlying = sp;
    }
  *tp = extend (*tp, ud, PLEXUS);

#undef SKIPWS
}

static void
getchaccess (struct adminstuff *dc,
             char const *login, enum changeaccess command)
{
  register struct chaccess *ch;
  struct link **tp = &dc->tp_access;

  if (! *tp)
    *tp = &dc->accesses;

  ch = ZLLOC (1, struct chaccess);
  ch->login = login;
  ch->command = command;
  *tp = extend (*tp, ch, PLEXUS);
}

static void
getaccessor (struct adminstuff *dc, char *opt, enum changeaccess command)
/* Get the accessor list of options ‘-e’ and ‘-a’; store in ‘chaccess’.  */
{
  register int c;
  register char *sp;

  sp = opt;
  while ((c = *++sp) == ' ' || c == '\n' || c == '\t' || c == ',')
    continue;
  if (c == '\0')
    {
      if (command == erase && sp - opt == 1)
        {
          getchaccess (dc, NULL, command);
          return;
        }
      PERR ("missing login name after option -a or -e");
      return;
    }

  while (c != '\0')
    {
      size_t len;

      getchaccess (dc, SHSNIP (&len, sp, checkid (sp, ',')), command);
      sp += len;
      c = *sp;
      while (c == ' ' || c == '\n' || c == '\t' || c == ',')
        c = (*++sp);
    }
}

static void
getmessage (struct adminstuff *dc, char *option)
{
  struct u_log *um;
  struct cbuf cb;
  char *m;
  struct link **tp = &dc->tp_log;

  if (! *tp)
    *tp = &dc->logs;

  if (!(m = strchr (option, ':')))
    {
      PERR ("-m option lacks %s", ks_revno);
      return;
    }
  *m++ = '\0';
  cb = cleanlogmsg (m, strlen (m));
  if (!cb.size)
    set_empty_log_message (&cb);
  um = ZLLOC (1, struct u_log);
  um->revno = option;
  um->message = cb;
  *tp = extend (*tp, um, PLEXUS);
}

static void
getstates (struct adminstuff *dc, char *sp)
/* Get one state attribute and the corresponding rev;
   store in ‘dc->states’.  */
{
  char const *temp;
  struct u_state *us;
  register int c;
  size_t len;
  struct link **tp = &dc->tp_state;

  if (! *tp)
    *tp = &dc->states;

  while ((c = *++sp) == ' ' || c == '\t' || c == '\n')
    continue;
  /* Check for invalid state attribute.  */
  temp = checkid (sp, ':');
  temp = SHSNIP (&len, sp, temp);
  sp += len;
  c = *sp;
  while (c == ' ' || c == '\t' || c == '\n')
    c = *++sp;

  if (c == '\0')
    {
      /* Change state of default branch or ‘REPO (tip)’.  */
      dc->headstate_changed_p = true;
      dc->headstate = temp;
      return;
    }
  else if (c != ':')
    {
      PERR ("missing ':' after state in option -s");
      return;
    }

  while ((c = *++sp) == ' ' || c == '\t' || c == '\n')
    continue;
  us = ZLLOC (1, struct u_state);
  us->status = temp;
  us->revno = sp;
  *tp = extend (*tp, us, PLEXUS);
}

static void
putdelrev (char const *b, char const *e, bool sawsep, void *data)
{
  struct adminstuff *dc = data;

  if (dc->delrev.strt || dc->delrev.end)
    {
      PWARN ("ignoring spurious `-o' range `%s:%s'",
             b ? b : "(unspecified)",
             e ? e : "(unspecified)");
      return;
    }

  if (!sawsep)
    /* -o rev or branch */
    {
      dc->delrev.strt = b;
      dc->delrev.code = 0;
    }
  else if (!b || !b[0])
    /* -o:rev */
    {
      dc->delrev.strt = e;              /* FIXME: weird */
      dc->delrev.code = 1;
    }
  else if (!e[0])
    /* -orev: */
    {
      dc->delrev.strt = b;
      dc->delrev.code = 2;
    }
  else
    /* -orev1:rev2 */
    {
      dc->delrev.strt = b;
      dc->delrev.end = e;
      dc->delrev.code = 3;
    }
}

static void
scanlogtext (struct adminstuff *dc,
             struct editstuff *es, struct wlink **ls,
             struct delta *delta, bool edit)
/* Scan delta text nodes up to and including the one given by ‘delta’,
   or up to last one present, if ‘!delta’.  For the one given by
   ‘delta’ (if ‘delta’), the log message is saved into ‘delta->pretty_log’ if
   ‘delta == dc->cuttail’; the text is edited if ‘edit’ is set, else copied.
   Do not advance input after finished, except if ‘!delta’.  */
{
  struct delta const *nextdelta;
  struct fro *from = FLOW (from);
  FILE *to;
  struct atat *log, *text;
  struct range range;

  for (;; *ls = (*ls)->next)
    {
      if (! *ls)
        /* No more.  */
        return;
      to = FLOW (to) = NULL;
      nextdelta = (*ls)->entry;
      log = nextdelta->log;
      text = nextdelta->text;
      range.beg = nextdelta->neck;
      if (nextdelta->selector)
        {
          to = FLOW (to) = FLOW (rewr);
          range.end = log->beg;
          fro_spew_partial (to, from, &range);
        }
      if (nextdelta == dc->cuttail)
        {
          if (!delta->pretty_log.string)
            {
              delta->pretty_log = string_from_atat (SINGLE, log);
              delta->pretty_log = cleanlogmsg (delta->pretty_log.string,
                                               delta->pretty_log.size);
            }
        }
      else if (nextdelta->pretty_log.string && nextdelta->selector)
        {
          afputc (SDELIM, to);
          putstring (to, false, nextdelta->pretty_log, true);
          afputc ('\n', to);
        }
      else if (to)
        atat_put (to, log);
      range.beg = ATAT_TEXT_END (log);
      range.end = text->beg;
      if (to)
        fro_spew_partial (to, from, &range);
      if (delta == nextdelta)
        break;
      /* Skip over it.  */
      if (to)
        atat_put (to, text);
    }
  /* Got the one we're looking for.  */
  fro_move (from, range.end);
  if (edit)
    editstring (es, text, NULL);
  else
    enterstring (es, text);
}

static struct link *
rmnewlocklst (struct adminstuff *dc, char const *which)
/* Remove lock to revision ‘which’ from ‘dc->newlocks’.  */
{
  struct link *pt, **pre;

  pre = &dc->newlocks;
  while ((pt = *pre))
    if (STR_DIFF (pt->entry, which))
      pre = &pt->next;
    else
      *pre = pt->next;
  return *pre;
}

static bool
doaccess (struct adminstuff *dc)
{
  register bool changed = false;
  struct link *ls, box, *tp;

  for (ls = dc->accesses.next; ls; ls = ls->next)
    {
      struct chaccess const *ch = ls->entry;
      char const *login = ch->login;

      switch (ch->command)
        {
        case erase:
          if (!login)
            {
              if (GROK (access))
                {
                  GROK (access) = NULL;
                  changed = true;
                }
            }
          else
            for (box.next = GROK (access), tp = &box;
                 tp->next; tp = tp->next)
              if (STR_SAME (login, tp->next->entry))
                {
                  tp->next = tp->next->next;
                  changed = true;
                  GROK (access) = box.next;
                  break;
                }
          break;
        case append:
          for (box.next = GROK (access), tp = &box;
               tp->next; tp = tp->next)
            if (STR_SAME (login, tp->next->entry))
              /* Do nothing; already present.  */
              break;
          if (!tp->next)
            {
              extend (tp, login, SINGLE);
              changed = true;
              GROK (access) = box.next;
            }
          break;
        }
    }
  return changed;
}

static bool
sendmail (char const *Delta, char const *who, bool suppress_mail)
/* Mail to ‘who’, informing him that his lock on ‘Delta’ was broken by
   caller.  Ask first whether to go ahead.  Return false on error or if
   user decides not to break the lock.  */
{
#ifdef SENDMAIL
  int old1, old2, c, status;
  FILE *mailmess;
#endif

  complain ("Revision %s is already locked by %s.\n", Delta, who);
  if (suppress_mail)
    return true;
  if (!yesorno (false, "Do you want to break the lock? [ny](n): "))
    return false;

  /* Go ahead with breaking.  */
#ifdef SENDMAIL
  if (! (mailmess = tmpfile ()))
    fatal_sys ("tmpfile");

  aprintf (mailmess,
           "Subject: Broken lock on %s\n\nYour lock on revision %s of file %s\nhas been broken by %s for the following reason:\n",
           basefilename (REPO (filename)), Delta, getfullRCSname (), getcaller ());
  complain ("%s\n%s\n>> ",
            "State the reason for breaking the lock:",
            "(terminate with single '.' or end of file)");

  old1 = '\n';
  old2 = ' ';
  for (;;)
    {
      c = getcstdin ();
      if (feof (stdin))
        {
          aprintf (mailmess, "%c\n", old1);
          break;
        }
      else if (c == '\n' && old1 == '.' && old2 == '\n')
        break;
      else
        {
          afputc (old1, mailmess);
          old2 = old1;
          old1 = c;
          if (c == '\n')
            complain (">> ");
        }
    }
  rewind (mailmess);
  aflush (mailmess);
  status = run (fileno (mailmess), NULL, SENDMAIL, who, NULL);
  Ozclose (&mailmess);
  if (status == 0)
    return true;
  PWARN ("Mail failed.");
#endif  /* defined SENDMAIL */
  PWARN ("Mail notification of broken locks is not available.");
  PWARN ("Please tell `%s' why you broke the lock.", who);
  return true;
}

static bool
breaklock (struct delta const *delta, bool suppress_mail)
/* Find the lock held by caller on ‘delta’, and remove it.
   Send mail if a lock different from the caller's is broken.
   Print an error message if there is no such lock or error.  */
{
  struct rcslock const *rl;
  struct link box, *tp;
  char const *num, *before;

  num = delta->num;
  box.next = GROK (locks);
  if (! (tp = lock_delta_memq (&box, delta)))
    {
      RERR ("no lock set on revision %s", num);
      return false;
    }
  rl = tp->next->entry;
  before = rl->login;
  if (!caller_login_p (before)
      && !sendmail (num, before, suppress_mail))
    {
      RERR ("revision %s still locked by %s", num, before);
      return false;
    }
  diagnose ("%s unlocked", num);
  lock_drop (&box, tp);
  return true;
}

static struct delta *
searchcutpt (struct adminstuff *dc,
             char const *object, int length, struct wlink *store)
/* Search store and return entry with number being ‘object’.
   ‘dc->cuttail’ is 0, if the entry is ‘REPO (tip)’; otherwise, it
   is the entry point to the one with number being ‘object’.  */
{
  struct delta *delta;

  dc->cuthead = NULL;
  while (delta = store->entry,
         compartial (delta->num, object, length))
    {
      dc->cuthead = delta;
      store = store->next;
    }
  return delta;
}

static bool
branchpoint (struct delta *strt, struct delta *tail)
/* Check whether the deltas between ‘strt’ and ‘tail’ are locked or
   branch point, return true if any is locked or branch point; otherwise,
   return false and mark deleted.  */
{
  struct delta *pt;

  for (pt = strt; pt != tail; pt = pt->ilk)
    {
      if (pt->branches)
        {
          /* A branch point.  */
          RERR ("can't remove branch point %s", pt->num);
          return true;
        }
      if (lock_on (pt))
        {
          RERR ("can't remove locked revision %s", pt->num);
          return true;
        }
      pt->selector = false;
      diagnose ("deleting revision %s", pt->num);
    }
  return false;
}

static bool
removerevs (struct adminstuff *dc)
/* Get the revision range to be removed, and place the first revision
   removed in ‘dc->delstrt’, the revision before ‘dc->delstrt’ in
   ‘dc->cuthead’ (NULL, if ‘dc->delstrt’ is head), and the revision
   after the last removed revision in ‘dc->cuttail’ (NULL if the last
   is a leaf).  */
{
  struct cbuf numrev;
  struct delta *target, *target2, *temp;
  struct wlink *ls;
  int length;
  int cmp;

#define GENREV(x)    gr_revno (x, &ls)
#define SEARCH(x,l)  searchcutpt (dc, x, l, ls)

  if (!fully_numeric_no_k (&numrev, dc->delrev.strt))
    return false;
  target = GENREV (numrev.string);
  if (!target)
    return false;
  cmp = cmpnum (target->num, numrev.string);
  length = countnumflds (numrev.string);

  if (dc->delrev.code == 0)
    {                           /* -o rev or -o branch */
      if (ODDP (length))
        temp = SEARCH (target->num, length + 1);
      else if (cmp)
        {
          RERR ("Revision %s doesn't exist.", numrev.string);
          return false;
        }
      else
        temp = SEARCH (numrev.string, length);
      dc->cuttail = target->ilk;
      if (branchpoint (temp, dc->cuttail))
        {
          dc->cuttail = NULL;
          return false;
        }
      dc->delstrt = temp;           /* first revision to be removed */
      return true;
    }

  if (ODDP (length))
    {                           /* invalid branch after -o */
      RERR ("invalid branch range %s after -o", numrev.string);
      return false;
    }

  if (dc->delrev.code == 1)
    {                           /* -o -rev */
      if (length > 2)
        {
          temp = SEARCH (target->num, length - 1);
          dc->cuttail = target->ilk;
        }
      else
        {
          temp = SEARCH (target->num, length);
          dc->cuttail = target;
          while (dc->cuttail && NUMF_EQ (1, target->num, dc->cuttail->num))
            dc->cuttail = dc->cuttail->ilk;
        }
      if (branchpoint (temp, dc->cuttail))
        {
          dc->cuttail = NULL;
          return false;
        }
      dc->delstrt = temp;
      return true;
    }

  if (dc->delrev.code == 2)
    {                           /* -o rev- */
      if (length == 2)
        {
          temp = SEARCH (target->num, 1);
          if (cmp)
            dc->cuttail = target;
          else
            dc->cuttail = target->ilk;
        }
      else
        {
          if (cmp)
            {
              dc->cuthead = target;
              if (!(temp = target->ilk))
                return false;
            }
          else
            temp = SEARCH (target->num, length);
          GENREV (BRANCHNO (temp->num));
        }
      if (branchpoint (temp, dc->cuttail))
        {
          dc->cuttail = NULL;
          return false;
        }
      dc->delstrt = temp;
      return true;
    }

  /* -o rev1-rev2 */
  if (!fully_numeric_no_k (&numrev, dc->delrev.end))
    return false;
  if (length != countnumflds (numrev.string)
      || (length > 2 && compartial (numrev.string, target->num, length - 1)))
    {
      RERR ("invalid revision range %s-%s", target->num, numrev.string);
      return false;
    }

  target2 = GENREV (numrev.string);
  if (!target2)
    return false;

  if (length > 2)
    {                           /* delete revisions on branches */
      if (NUM_GT (target->num, target2->num))
        {
          cmp = cmpnum (target2->num, numrev.string);
          temp = target;
          target = target2;
          target2 = temp;
        }
      if (cmp)
        {
          if (NUM_EQ (target->num, target2->num))
            {
              RERR ("Revisions %s-%s don't exist.",
                    dc->delrev.strt, dc->delrev.end);
              return false;
            }
          dc->cuthead = target;
          temp = target->ilk;
        }
      else
        temp = SEARCH (target->num, length);
      dc->cuttail = target2->ilk;
    }
  else
    {                           /* delete revisions on trunk */
      if (NUM_LT (target->num, target2->num))
        {
          temp = target;
          target = target2;
          target2 = temp;
        }
      else
        cmp = cmpnum (target2->num, numrev.string);
      if (cmp)
        {
          if (NUM_EQ (target->num, target2->num))
            {
              RERR ("Revisions %s-%s don't exist.",
                    dc->delrev.strt, dc->delrev.end);
              return false;
            }
          dc->cuttail = target2;
        }
      else
        dc->cuttail = target2->ilk;
      temp = SEARCH (target->num, length);
    }
  if (branchpoint (temp, dc->cuttail))
    {
      dc->cuttail = NULL;
      return false;
    }
  dc->delstrt = temp;
  return true;

#undef SEARCH
#undef GENREV
}

static bool
doassoc (struct adminstuff *dc)
/* Add or delete (if !underlying) association
   that is stored in ‘dc->assocs’.  */
{
  struct cbuf numrev;
  char const *p;
  bool changed = false;

  for (struct link *cur = dc->assocs.next; cur; cur = cur->next)
    {
      struct u_symdef const *u = cur->entry;
      char const *ssymbol = u->u.meaningful;
      char const *under = u->u.underlying;

      if (!under)
        /* Delete symbol.  */
        {
          struct link box, *tp;
          struct symdef const *d = NULL;

          for (box.next = GROK (symbols), tp = &box; tp->next; tp = tp->next)
            {
              d = tp->next->entry;
              if (STR_SAME (ssymbol, d->meaningful))
                {
                  tp->next = tp->next->next;
                  changed = true;
                  break;
                }
            }
          GROK (symbols) = box.next;
          if (!d)
            RWARN ("can't delete nonexisting symbol %s", ssymbol);
        }
      else
        /* Add new association.  */
        {
          if (under[0])
            p = fully_numeric_no_k (&numrev, under)
              ? numrev.string
              : NULL;
          else if (!(p = tiprev ()))
            RERR ("no latest revision to associate with symbol %s", ssymbol);
          if (p)
            changed |= addsymbol (p, ssymbol, u->override);
        }
    }
  return changed;
}

static bool
setlock (struct adminstuff *dc, char const *rev)
/* Given a revision or branch number, find the corresponding
   delta and lock it for caller.  */
{
  struct cbuf numrev;
  struct delta *target;
  int r;

  if (fully_numeric_no_k (&numrev, rev))
    {
      target = gr_revno (numrev.string, &dc->deltas);
      if (target)
        {
          if (EVENP (countnumflds (numrev.string))
              && !NUM_EQ (target->num, numrev.string))
            RERR ("can't lock nonexisting revision %s", numrev.string);
          else
            {
              if ((r = addlock (target, false)) < 0
                  && breaklock (target, dc->suppress_mail))
                r = addlock (target, true);
              if (0 <= r)
                {
                  if (r)
                    diagnose ("%s locked", target->num);
                  return r;
                }
            }
        }
    }
  return false;
}

static bool
dolocks (struct adminstuff *dc)
/* Remove lock for caller or first lock if ‘dc->unlockcaller’ is set;
   remove locks which are stored in ‘dc->byelocks’,
   add new locks which are stored in ‘dc->newlocks’,
   add lock for ‘GROK (branch)’ or ‘REPO (tip)’ if ‘dc->lockhead’ is set.  */
{
  struct cbuf numrev;
  struct link const *lockpt;
  struct delta *target, *tip = REPO (tip);
  bool changed = false;
  const char *bye;

  if (dc->unlockcaller)
    {
      /* Find lock for caller.  */
      if (tip)
        {
          struct link *locks = GROK (locks);

          if (locks)
            {
              switch (findlock (true, &target))
                {
                case 0:
                  /* Remove most recent lock.  */
                  {
                    struct rcslock const *rl = locks->entry;

                    changed |= breaklock (rl->delta, dc->suppress_mail);
                  }
                  break;
                case 1:
                  diagnose ("%s unlocked", target->num);
                  changed = true;
                  break;
                }
            }
          else
            {
              RWARN ("No locks are set.");
            }
        }
      else
        {
          RWARN ("can't unlock an empty tree");
        }
    }

  /* Remove locks which are stored in ‘dc->byelocks’.  */
  for (lockpt = dc->byelocks; lockpt; lockpt = lockpt->next)
    if (fully_numeric_no_k (&numrev, (bye = lockpt->entry)))
      {
        target = gr_revno (numrev.string, &dc->deltas);
        if (target)
          {
            if (EVENP (countnumflds (numrev.string))
                && !NUM_EQ (target->num, numrev.string))
              RERR ("can't unlock nonexisting revision %s", bye);
            else
              changed |= breaklock (target, dc->suppress_mail);
          }
        /* ‘breaklock’ does its own ‘diagnose’.  */
      }

  /* Add new locks which stored in ‘dc->newlocks’.  */
  for (lockpt = dc->newlocks; lockpt; lockpt = lockpt->next)
    changed |= setlock (dc, lockpt->entry);

  if (dc->lockhead)
    {
      char const *defbr = GROK (branch);

      /* Lock default branch or head.  */
      if (defbr)
        changed |= setlock (dc, defbr);
      else if (tip)
        changed |= setlock (dc, tip->num);
      else
        RWARN ("can't lock an empty tree");
    }
  return changed;
}

static bool
domessages (struct adminstuff *dc)
{
  struct delta *target;
  bool changed = false;

  for (struct link *ls = dc->logs.next; ls; ls = ls->next)
    {
      struct u_log const *um = ls->entry;
      struct cbuf numrev;

      if (fully_numeric_no_k (&numrev, um->revno)
          && (target = gr_revno (numrev.string, &dc->deltas)))
        {
          /* We can't check the old log -- it's much later in the file.
             We pessimistically assume that it changed.  */
          target->pretty_log = um->message;
          changed = true;
        }
    }
  return changed;
}

static bool
rcs_setstate (struct adminstuff *dc,
              char const *rev, char const *status)
/* Given a revision or branch number, find the corresponding delta
   and sets its state to ‘status’.  */
{
  struct cbuf numrev;
  struct delta *target;

  if (fully_numeric_no_k (&numrev, rev))
    {
      target = gr_revno (numrev.string, &dc->deltas);
      if (target)
        {
          if (EVENP (countnumflds (numrev.string))
              && !NUM_EQ (target->num, numrev.string))
            RERR ("can't set state of nonexisting revision %s", numrev.string);
          else if (STR_DIFF (target->state, status))
            {
              target->state = status;
              return true;
            }
        }
    }
  return false;
}

static bool
buildeltatext (struct adminstuff *dc,
               struct editstuff *es, struct wlink **ls,
               struct wlink const *deltas)
/* Put the delta text on ‘FLOW (rewr)’ and make necessary
   change to delta text.  */
{
  FILE *fcut;                       /* temporary file to rebuild delta tree */
  FILE *frew = FLOW (rewr);

  fcut = NULL;
  dc->cuttail->selector = false;
  scanlogtext (dc, es, ls, deltas->entry, false);
  if (dc->cuthead)
    {
      if (! (fcut = tmpfile ()))
        fatal_sys ("tmpfile");

      while (deltas->entry != dc->cuthead)
        {
          *ls = (*ls)->next;
          deltas = deltas->next;
          scanlogtext (dc, es, ls, deltas->entry, true);
        }

      snapshotedit (es, fcut);
      rewind (fcut);
      aflush (fcut);
    }

  while (deltas->entry != dc->cuttail)
    {
      *ls = (*ls)->next;
      deltas = deltas->next;
      scanlogtext (dc, es, ls, deltas->entry, true);
    }
  finishedit (es, NULL, NULL, true);
  Ozclose (&FLOW (res));

  if (fcut)
    {
      char const *diffname = maketemp (0);
      char const *diffv[6 + !!OPEN_O_BINARY];
      char const **diffp = diffv;

      *++diffp = prog_diff;
      *++diffp = diff_flags;
#if OPEN_O_BINARY
      if (BE (kws) == kwsub_b)
        *++diffp == "--binary";
#endif
      *++diffp = "-";
      *++diffp = FLOW (result);
      *++diffp = '\0';
      if (DIFF_TROUBLE == runv (fileno (fcut), diffname, diffv))
        RFATAL ("diff failed");
      Ozclose (&fcut);
      return putdtext (dc->cuttail, diffname, frew, true);
    }
  else
    return putdtext (dc->cuttail, FLOW (result), frew, false);
}

static void
buildtree (struct adminstuff *dc)
/* Actually remove revisions whose selector field
   is false, and rebuild the linkage of deltas.
   Ask for reconfirmation if deleting last revision.  */
{
  struct delta *Delta;

  if (dc->cuthead)
    if (dc->cuthead->ilk == dc->delstrt)
      dc->cuthead->ilk = dc->cuttail;
    else
      {
        struct wlink *pt = dc->cuthead->branches, *pre = pt;

        while (pt && pt->entry != dc->delstrt)
          {
            pre = pt;
            pt = pt->next;
          }
        if (dc->cuttail)
          pt->entry = dc->cuttail;
        else if (pt == pre)
          dc->cuthead->branches = pt->next;
        else
          pre->next = pt->next;
      }
  else
    {
      if (!dc->cuttail && !BE (quiet))
        {
          if (!yesorno
              (false,
               "Do you really want to delete all revisions? [ny](n): "))
            {
              RERR ("No revision deleted");
              Delta = dc->delstrt;
              while (Delta)
                {
                  Delta->selector = true;
                  Delta = Delta->ilk;
                }
              return;
            }
        }
      REPO (tip) = dc->cuttail;
    }
  return;
}

int
rcs_main (const char *cmd, int argc, char **argv)
{
  struct adminstuff dc;                 /* dynamic context */
  char *a, **newargv, *textfile;
  char const *branchsym, *commsyml;
  bool branchflag, initflag, textflag;
  int changed, expmode;
  bool strictlock, strict_selected, Ttimeflag;
  bool keepRCStime;
  size_t commsymlen;
  struct cbuf branchnum;
  struct link boxlock, *tplock;
  struct link boxrm, *tprm;
  const struct program program =
    {
      .invoke = argv[0],
      .name = cmd,
      .help = rcs_help,
      .tyag = BOG_FULL
    };

  CHECK_HV ();
  gnurcs_init (&program);
  memset (&dc, 0, sizeof (dc));
  dc.rv = EXIT_SUCCESS;

  nosetid ();

  branchsym = commsyml = textfile = NULL;
  branchflag = strictlock = false;
  commsymlen = 0;
  boxlock.next = dc.newlocks;
  tplock = &boxlock;
  boxrm.next = dc.byelocks;
  tprm = &boxrm;
  expmode = -1;
  BE (pe) = X_DEFAULT;
  initflag = textflag = false;
  strict_selected = false;
  Ttimeflag = false;

  /* Preprocess command options.  */
  if (1 < argc && argv[1][0] != '-')
    PWARN ("No options were given; this usage is obsolescent.");

  argc = getRCSINIT (argc, argv, &newargv);
  argv = newargv;
  while (a = *++argv, 0 < --argc && *a++ == '-')
    {
      switch (*a++)
        {

        case 'i':
          /* Initial version.  */
          initflag = true;
          break;

        case 'b':
          /* Change default branch.  */
          if (branchflag)
            redefined ('b');
          branchflag = true;
          branchsym = a;
          break;

        case 'c':
          /* Change comment symbol.  */
          if (commsyml)
            redefined ('c');
          commsyml = a;
          commsymlen = strlen (a);
          break;

        case 'a':
          /* Add new accessor.  */
          getaccessor (&dc, *argv + 1, append);
          break;

        case 'A':
          /* Append access list according to accessfile.  */
          if (!*a)
            {
              PERR ("missing filename after -A");
              break;
            }
          *argv = a;
          if (0 < pairnames (1, argv, rcsreadopen, true, false))
            {
              for (struct link *ls = GROK (access); ls; ls = ls->next)
                getchaccess (&dc, str_save (ls->entry), append);
              fro_zclose (&FLOW (from));
            }
          break;

        case 'e':
          /* Remove accessors.  */
          getaccessor (&dc, *argv + 1, erase);
          break;

        case 'l':
          /* Lock a revision if it is unlocked.  */
          if (!*a)
            {
              /* Lock head or default branch.  */
              dc.lockhead = true;
              break;
            }
          tplock = extend (tplock, a, PLEXUS);
          break;

        case 'u':
          /* Release lock of a locked revision.  */
          if (!*a)
            {
              dc.unlockcaller = true;
              break;
            }
          tprm = extend (tprm, a, PLEXUS);
          dc.newlocks = boxlock.next;
          tplock = rmnewlocklst (&dc, a);
          break;

        case 'L':
          /* Set strict locking.  */
          if (strict_selected)
            {
              if (!strictlock)
                PWARN ("-U overridden by -L");
            }
          strictlock = true;
          strict_selected = true;
          break;

        case 'U':
          /* Release strict locking.  */
          if (strict_selected)
            {
              if (strictlock)
                PWARN ("-L overridden by -U");
            }
          strict_selected = true;
          break;

        case 'n':
          /* Add new association: error, if name exists.  */
        case 'N':
          /* Add or change association.  */
          if (!*a)
            {
              PERR ("missing symbolic name after -%c", (*argv)[1]);
              break;
            }
          getassoclst (&dc, (*argv) + 1);
          break;

        case 'm':
          /* Change log message.  */
          getmessage (&dc, a);
          break;

        case 'M':
          /* Do not send mail.  */
          dc.suppress_mail = true;
          break;

        case 'o':
          /* Delete revisions.  */
          if (dc.delrev.strt)
            redefined ('o');
          if (!*a)
            {
              PERR ("missing revision range after -o");
              break;
            }
          parse_revpairs ('o', (*argv) + 2, &dc, putdelrev);
          break;

        case 's':
          /* Change state attribute of a revision.  */
          if (!*a)
            {
              PERR ("state missing after -s");
              break;
            }
          getstates (&dc, (*argv) + 1);
          break;

        case 't':
          /* Change descriptive text.  */
          textflag = true;
          if (*a)
            {
              if (textfile)
                redefined ('t');
              textfile = a;
            }
          break;

        case 'T':
          /* Do not update last-mod time for minor changes.  */
          if (*a)
            goto unknown;
          Ttimeflag = true;
          break;

        case 'I':
          BE (interactive) = true;
          break;

        case 'q':
          BE (quiet) = true;
          break;

        case 'x':
          BE (pe) = a;
          break;

        case 'V':
          setRCSversion (*argv);
          break;

        case 'z':
          zone_set (a);
          break;

        case 'k':
          /* Set keyword expand mode.  */
          if (0 <= expmode)
            redefined ('k');
          if (0 <= (expmode = str2expmode (a)))
            break;
          /* fall into */
        default:
        unknown:
          bad_option (*argv);
        };
    }
  dc.newlocks = boxlock.next;
  dc.byelocks = boxrm.next;
  /* (End processing of options.)  */

  /* Now handle all filenames.  */
  if (FLOW (erroneousp))
    cleanup (&dc.rv);
  else if (argc < 1)
    PFATAL ("no input file");
  else
    for (; 0 < argc; cleanup (&dc.rv), ++argv, --argc)
      {
        struct delta *tip;
        char const *defbr;
        struct stat *repo_stat;
        struct cbuf newdesc =
          {
            .string = NULL,
            .size = 0
          };

        ffree ();

        if (initflag)
          {
            switch (pairnames (argc, argv, rcswriteopen, false, false))
              {
              case -1:
                break;          /* not exist; ok */
              case 0:
                continue;       /* error */
              case 1:
                RERR ("already exists");
                continue;
              }
          }
        else
          {
            switch (pairnames (argc, argv, rcswriteopen, true, false))
              {
              case -1:
                continue;       /* not exist */
              case 0:
                continue;       /* errors */
              case 1:
                break;          /* file exists; ok */
              }
          }

        /* ‘REPO (filename)’ contains the name of the RCS file, and
           ‘MANI (filename)’ contains the name of the working file.
           If ‘!initflag’, ‘FLOW (from)’ contains the file descriptor
           for the RCS file.  The admin node is initialized.  */
        repo_stat = &REPO (stat);
        tip = REPO (tip);
        defbr = GROK (branch);
        diagnose ("RCS file: %s", REPO (filename));

        changed = initflag | textflag;
        keepRCStime = Ttimeflag;
        if (!initflag)
          {
            if (!checkaccesslist ())
              continue;
          }

        /* Update admin. node.  */
        if (strict_selected)
          {
            changed |= BE (strictly_locking) ^ strictlock;
            BE (strictly_locking) = strictlock;
          }
        if (commsyml &&
            (commsymlen != REPO (log_lead).size
             || MEM_DIFF (commsymlen, commsyml, REPO (log_lead).string)))
          {
            REPO (log_lead).string = commsyml;
            REPO (log_lead).size = commsymlen;
            changed = true;
          }
        if (0 <= expmode && BE (kws) != expmode)
          {
            BE (kws) = expmode;
            changed = true;
          }

        /* Update default branch.  */
        if (branchflag && fully_numeric_no_k (&branchnum, branchsym))
          {
            if (countnumflds (branchnum.string))
              {
                if (!NUM_EQ (defbr, branchnum.string))
                  {
                    defbr = GROK (branch) = branchnum.string;
                    changed = true;
                  }
              }
            else if (defbr)
              {
                defbr = GROK (branch) = NULL;
                changed = true;
              }
          }

        /* Update access list.  */
        changed |= doaccess (&dc);

        /* Update association list.  */
        changed |= doassoc (&dc);

        /* Update locks.  */
        changed |= dolocks (&dc);

        /* Update log messages.  */
        changed |= domessages (&dc);

        /* Update state attribution.  */
        if (dc.headstate_changed_p)
          {
            /* Change state of default branch or head.  */
            if (!defbr)
              {
                if (!tip)
                  RWARN ("can't change states in an empty tree");
                else if (STR_DIFF (tip->state, dc.headstate))
                  {
                    tip->state = dc.headstate;
                    changed = true;
                  }
              }
            else
              changed |= rcs_setstate (&dc, defbr, dc.headstate);
          }
        for (struct link *ls = dc.states.next; ls; ls = ls->next)
          {
            struct u_state const *us = ls->entry;

            changed |= rcs_setstate (&dc, us->revno, us->status);
          }

        dc.cuttail = NULL;
        if (dc.delrev.strt && removerevs (&dc))
          {
            /* Rebuild delta tree if some deltas are deleted.  */
            if (dc.cuttail)
              gr_revno (dc.cuttail->num, &dc.deltas);
            buildtree (&dc);
            tip = REPO (tip);
            changed = true;
            keepRCStime = false;
          }

        if (FLOW (erroneousp))
          continue;

        putadmin ();
        if (tip)
          puttree (tip, FLOW (rewr));
        putdesc (&newdesc, textflag, textfile);

        /* Don't conditionalize on non-NULL ‘REPO (tip)’; that prevents
           ‘scanlogtext’ from advancing the input pointer to EOF, in
           the process "marking" the intervening log messages to be
           discarded later.  The result is bogus log messages.  See
           <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=69193>.  */
        if (1)
          {
            if (dc.delrev.strt || dc.logs.next)
              {
                struct fro *from = FLOW (from);
                struct editstuff *es = make_editstuff ();
                struct wlink *ls = GROK (deltas);

                if (!dc.cuttail || buildeltatext (&dc, es, &ls, dc.deltas))
                  {
                    fro_trundling (true, from);
                    if (dc.cuttail)
                      ls = ls->next;
                    scanlogtext (&dc, es, &ls, NULL, false);
                    /* Copy rest of delta text nodes that are not deleted.  */
                    changed = true;
                  }
                unmake_editstuff (es);
                IGNORE_REST (from);
              }
            else if (GROK (desc))
              SAME_AFTER (FLOW (from), GROK (desc));
          }

        if (initflag)
          {
            /* Adjust things for donerewrite's sake.  */
            if (PROB (stat (MANI (filename), repo_stat)))
              {
#if BAD_CREAT0
                mode_t m = umask (0);
                umask (m);
                repo_stat->st_mode = (S_IRUSR | S_IRGRP | S_IROTH) & ~m;
#else
                changed = -1;
#endif
              }
            repo_stat->st_nlink = 0;
            keepRCStime = false;
          }
        if (PROB (donerewrite (changed, keepRCStime
                               ? repo_stat->st_mtime
                               : (time_t) - 1)))
          break;

        diagnose ("done");
      }

  tempunlink ();
  gnurcs_goodbye ();
  return dc.rv;
}

const uint8_t rcs_aka[16] =
{
  3 /* count */,
  4,'f','r','o','b',
  3,'r','c','s',
  5,'a','d','m','i','n'
};

/*:help
[options] file ...
Options:
  -i              Create and initialize a new RCS file.
  -L              Set strict locking.
  -U              Set non-strict locking.
  -M              Don't send mail when breaking someone else's lock.
  -T              Preserve the modification time on the
                  RCS file unless a revision is removed.
  -I              Interactive.
  -q              Quiet mode.
  -aLOGINS        Append LOGINS (comma-separated) to access-list.
  -e[LOGINS]      Erase LOGINS (all if unspecified) from access-list.
  -AFILENAME      Append access-list of FILENAME to current access-list.
  -b[REV]         Set default branch to that of REV or
                  highest branch on trunk if REV is omitted.
  -l[REV]         Lock revision REV.
  -u[REV]         Unlock revision REV.
  -cSTRING        Set comment leader to STRING; don't use: obsolete.
  -kSUBST         Set default keyword substitution to SUBST (see co(1)).
  -mREV:MSG       Replace REV's log message with MSG.
  -nNAME[:[REV]]  If :REV is omitted, delete symbolic NAME.
                  Otherwise, associate NAME with REV; NAME must be new.
  -NNAME[:[REV]]  Like -n, but overwrite any previous assignment.
  -oRANGE         Outdate revisions in RANGE:
                    REV       -- single revision
                    BR        -- latest revision on branch BR
                    REV1:REV2 -- REV1 to REV2 on same branch
                    :REV      -- beginning of branch to REV
                    REV:      -- REV to end of branch
  -sSTATE[:REV]   Set state of REV to STATE.
  -t-TEXT         Set description in RCS file to TEXT.
  -tFILENAME      Set description in RCS file to contents of FILENAME.
  -V              Like --version.
  -VN             Emulate RCS version N.
  -xSUFF          Specify SUFF as a slash-separated list of suffixes
                  used to identify RCS file names.
  -zZONE          No effect; included for compatibility with other commands.

REV defaults to the latest revision on the default branch.
*/

/* rcs.c ends here */

/* Check in revisions of RCS files from working files.

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
#include <errno.h>
#include <ctype.h>                      /* isdigit */
#include <stdlib.h>
#include <unistd.h>
#include "same-inode.h"
#include "ci.help"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-esds.h"
#include "b-excwho.h"
#include "b-fb.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-grok.h"
#include "b-isr.h"
#include "b-kwxout.h"

/* Work around a common ‘ftruncate’ bug: NFS won't let you truncate a file
   that you currently lack permissions for, even if you had permissions when
   you opened it.  Also, POSIX 1003.1b-1993 sec 5.6.7.2 p 128 l 1022 says
   ftruncate might fail because it's not supported.  */
#ifndef HAVE_FTRUNCATE
#undef ftruncate
#define ftruncate(fd,length) (-1)
#endif

struct reason
{
  struct cbuf upfront;                  /* from -mMSG */
  struct cbuf delayed;                  /* when the user is lazy */
};

struct work
{
  struct stat st;
  struct fro *fro;
  FILE *ex;                             /* expansion */
};

struct bud                              /* new growth */
{
  struct cbuf num;                      /* wip revision number */
  struct delta d;                       /* to be inserted */
  struct wlink br;                      /* branch to be inserted */
  bool keep;
  struct delta *target;
  char getcurdate_buffer[datesize];
};

static void
cleanup (int *exitstatus, struct work *work)
{
  if (FLOW (erroneousp))
    *exitstatus = EXIT_FAILURE;
  fro_zclose (&FLOW (from));
  fro_zclose (&work->fro);
  Ozclose (&work->ex);
  Ozclose (&FLOW (res));
  ORCSclose ();
  dirtempunlink ();
}

#define ACCF(...)  accf (PLEXUS, __VA_ARGS__)

#define OK(x)     (x)->string = finish_string (PLEXUS, &((x)->size))
#define JAM(x,s)  do { ACCF ("%s", s); OK (x); } while (0)
#define ADD(x,s)  do { ACCF ("%s", (x)->string); JAM (x, s); } while (0)

static void
incnum (char const *onum, struct cbuf *nnum)
/* Increment the last field of revision number ‘onum’
   by one into a ‘PLEXUS’ string and point ‘nnum’ at it.  */
{
  register char *tp, *np;
  register size_t l;

  ACCF ("%s%c", onum, '\0');
  np = finish_string (PLEXUS, &nnum->size);
  nnum->string = np;
  l = nnum->size - 1;
  for (tp = np + l; np != tp;)
    if (isdigit (*--tp))
      {
        if (*tp != '9')
          {
            ++*tp;
            nnum->size--;
            return;
          }
        *tp = '0';
      }
    else
      {
        tp++;
        break;
      }
  /* We changed 999 to 000; now change it to 1000.  */
  *tp = '1';
  tp = np + l;
  *tp++ = '0';
  *tp = '\0';
}

static int
removelock (struct delta *delta)
/* Find the lock held by caller on ‘delta’,
   remove it, and return nonzero if successful.
   Print an error message and return -1 if there is no such lock.
   An exception is if ‘!strictly_locking’, and caller is the owner of
   the RCS file.  If caller does not have a lock in this case,
   return 0; return 1 if a lock is actually removed.  */
{
  struct link box, *tp;
  struct rcslock const *rl;
  char const *num;

  num = delta->num;
  box.next = GROK (locks);
  if (! (tp = lock_delta_memq (&box, delta)))
    {
      if (!BE (strictly_locking) && stat_mine_p (&REPO (stat)))
        return 0;
      RERR ("no lock set by %s for revision %s", getcaller (), num);
      return -1;
    }
  rl = tp->next->entry;
  if (! caller_login_p (rl->login))
    {
      RERR ("revision %s locked by %s", num, rl->login);
      return -1;
    }
  /* We found a lock on ‘delta’ by caller; delete it.  */
  lock_drop (&box, tp);
  return 1;
}

static int
addbranch (struct delta *branchpoint, struct bud *bud,
           int removedlock, struct wlink **tp_deltas)
/* Add a new branch and branch delta at ‘branchpoint’.
   If ‘num’ is the null string, append the new branch, incrementing
   the highest branch number (initially 1), and setting the level number to 1.
   The new delta and branchhead are in ‘bud->d’ and ‘bud->br’, respectively.
   The new number is placed into a ‘PLEXUS’ string with ‘num’ pointing to it.
   Return -1 on error, 1 if a lock is removed, 0 otherwise.
   If ‘removedlock’ is 1, a lock was already removed.  */
{
  struct cbuf *num = &bud->num;
  struct wlink **btrail;
  struct delta *d;
  int result;
  int field, numlength;

  numlength = countnumflds (num->string);

  if (!branchpoint->branches)
    {
      /* Start first branch.  */
      branchpoint->branches = &bud->br;
      if (numlength == 0)
        {
          JAM (num, branchpoint->num);
          ADD (num, ".1.1");
        }
      else if (ODDP (numlength))
        ADD (num, ".1");
      bud->br.next = NULL;
    }
  else if (numlength == 0)
    {
      struct wlink *bhead = branchpoint->branches;

      /* Append new branch to the end.  */
      while (bhead->next)
        bhead = bhead->next;
      bhead->next = &bud->br;
      d = bhead->entry;
      incnum (BRANCHNO (d->num), num);
      ADD (num, ".1");
      bud->br.next = NULL;
    }
  else
    {
      /* Place the branch properly.  */
      field = numlength - EVENP (numlength);
      /* Field of branch number.  */
      btrail = &branchpoint->branches;
      while (d = (*btrail)->entry,
             0 < (result = cmpnumfld (num->string, d->num, field)))
        {
          btrail = &(*btrail)->next;
          if (!*btrail)
            {
              result = -1;
              break;
            }
        }
      if (result < 0)
        {
          /* Insert/append new branchhead.  */
          bud->br.next = *btrail;
          *btrail = &bud->br;
          if (ODDP (numlength))
            ADD (num, ".1");
        }
      else
        {
          /* Branch exists; append to end.  */
          bud->target = gr_revno (BRANCHNO (num->string), tp_deltas);
          if (!bud->target)
            return -1;
          if (!NUM_GT (num->string, bud->target->num))
            {
              RERR ("revision %s too low; must be higher than %s",
                    num->string, bud->target->num);
              return -1;
            }
          if (!removedlock && 0 <= (removedlock = removelock (bud->target)))
            {
              if (ODDP (numlength))
                incnum (bud->target->num, num);
              bud->target->ilk = &bud->d;
              bud->d.ilk = NULL;
            }
          return removedlock;
          /* Don't do anything to ‘bud->br’.  */
        }
    }
  bud->br.entry = &bud->d;
  bud->d.ilk = NULL;
  if (branchpoint->lockedby)
    if (caller_login_p (branchpoint->lockedby))
      return removelock (branchpoint);  /* This returns 1.  */
  return removedlock;
}

static void
prune (struct delta *wrong, struct delta *bp)
/* Remove reference to ‘wrong’ from the tree, starting the
   search at ‘bp’.  As a side effect, clear ‘wrong->selector’.  */
{
  struct wlink box, *tp;
  struct delta *d;
  int same = countnumflds (wrong->num) - 2;

  /* Unselecting ‘wrong’ is not strictly necessary,
     but doing so is cleaner.  */
  wrong->selector = false;

  /* On the trunk, no one points to ‘wrong’, so we are finished.  */
  if (0 >= same)
    return;

  /* If ‘wrong’ is the only revision on a branch, delete that branch.  */
  box.next = bp->branches;
  for (tp = &box; tp->next; tp = tp->next)
    if (wrong == (d = tp->next->entry))
      {
        tp->next = tp->next->next;
        bp->branches = box.next;
        return;
      }

  /* Otherwise, it must be on normal chain.  */
  for (tp = bp->branches; tp; tp = tp->next)
    {
      d = tp->entry;
      if (0 == compartial (wrong->num, d->num, same))
        {
          while (d->ilk != wrong)
            d = d->ilk;
          d->ilk = NULL;
          return;
        }
    }

  /* Should never get here.  */
  abort ();
}

static int
addelta (struct wlink **tp_deltas, struct bud *bud, bool rcsinitflag)
/* Append a delta to the delta tree, whose number is given by ‘bud->num’.
   Update ‘REPO (tip)’, ‘bud->num’ and the links in ‘bud->d’.
   Return -1 on error, 1 if a lock is removed, 0 otherwise.  */
{
  register char const *tp;
  register int i;
  int removedlock;
  int newdnumlength;            /* actual length of new rev. num. */
  struct delta *tip = REPO (tip);
  char const *defbr = GROK (branch);

  newdnumlength = countnumflds (bud->num.string);

  if (rcsinitflag)
    {
      /* This covers non-existing RCS file,
         and a file initialized with ‘rcs -i’.  */
      if (newdnumlength == 0 && defbr)
        {
          JAM (&bud->num, defbr);
          newdnumlength = countnumflds (defbr);
        }
      if (newdnumlength == 0)
        JAM (&bud->num, "1.1");
      else if (newdnumlength == 1)
        ADD (&bud->num, ".1");
      else if (newdnumlength > 2)
        {
          RERR ("Branch point doesn't exist for revision %s.",
                bud->num.string);
          return -1;
        }
      /* (‘newdnumlength’ == 2 is OK.)  */
      tip = REPO (tip) = &bud->d;
      bud->d.ilk = NULL;
      return 0;
    }
  if (newdnumlength == 0)
    {
      /* Derive new revision number from locks.  */
      switch (findlock (true, &bud->target))
        {

        default:
          /* Found two or more old locks.  */
          return -1;

        case 1:
          /* Found an old lock.  Check whether locked revision exists.  */
          if (!gr_revno (bud->target->num, tp_deltas))
            return -1;
          if (bud->target == tip)
            {
              /* Make new head.  */
              bud->d.ilk = tip;
              tip = REPO (tip) = &bud->d;
            }
          else if (!bud->target->ilk && countnumflds (bud->target->num) > 2)
            {
              /* New tip revision on side branch.  */
              bud->target->ilk = &bud->d;
              bud->d.ilk = NULL;
            }
          else
            {
              /* Middle revision; start a new branch.  */
              JAM (&bud->num, "");
              return addbranch (bud->target, bud, true, tp_deltas);
            }
          incnum (bud->target->num, &bud->num);
          /* Successful use of existing lock.  */
          return 1;

        case 0:
          /* No existing lock; try ‘defbr’.  Update ‘bud->num’.  */
          if (BE (strictly_locking) || !stat_mine_p (&REPO (stat)))
            {
              RERR ("no lock set by %s", getcaller ());
              return -1;
            }
          if (defbr)
            JAM (&bud->num, defbr);
          else
            {
              incnum (tip->num, &bud->num);
            }
          newdnumlength = countnumflds (bud->num.string);
          /* Now fall into next statement.  */
        }
    }
  if (newdnumlength <= 2)
    {
      /* Add new head per given number.  */
      if (newdnumlength == 1)
        {
          /* Make a two-field number out of it.  */
          if (NUMF_EQ (1, bud->num.string, tip->num))
            incnum (tip->num, &bud->num);
          else
            ADD (&bud->num, ".1");
        }
      if (!NUM_GT (bud->num.string, tip->num))
        {
          RERR ("revision %s too low; must be higher than %s",
                bud->num.string, tip->num);
          return -1;
        }
      bud->target = tip;
      if (0 <= (removedlock = removelock (tip)))
        {
          if (!gr_revno (tip->num, tp_deltas))
            return -1;
          bud->d.ilk = tip;
          tip = REPO (tip) = &bud->d;
        }
      return removedlock;
    }
  else
    {
      struct cbuf old = bud->num;       /* sigh */

      /* Put new revision on side branch.  First, get branch point.  */
      tp = old.string;
      for (i = newdnumlength - EVENP (newdnumlength); --i;)
        while (*tp++ != '.')
          continue;
      /* Ignore rest to get old delta.  */
      old.string = SHSNIP (&old.size, old.string, tp - 1);
      if (! (bud->target = gr_revno (old.string, tp_deltas)))
        return -1;
      if (!NUM_EQ (bud->target->num, old.string))
        {
          RERR ("can't find branch point %s", old.string);
          return -1;
        }
      return addbranch (bud->target, bud, false, tp_deltas);
    }
}

static bool
addsyms (char const *num, struct link *ls)
{
  struct u_symdef const *ud;

  for (; ls; ls = ls->next)
    {
      ud = ls->entry;

      if (addsymbol (num, ud->u.meaningful, ud->override) < 0)
        return false;
    }
  return true;
}

static char const *
getcurdate (struct bud *bud)
/* Return a pointer to the current date.  */
{
  if (!bud->getcurdate_buffer[0])
    time2date (BE (now), bud->getcurdate_buffer);
  return bud->getcurdate_buffer;
}

static int
fixwork (mode_t newworkmode, time_t mtime, struct work *work)
{
  char const *mani_filename = MANI (filename);
  struct stat *st = &work->st;

  return
    1 < st->st_nlink
    || (newworkmode & S_IWUSR && !stat_mine_p (st))
    || PROB (setmtime (mani_filename, mtime))
    ? -1 : st->st_mode == newworkmode ? 0
    : !PROB (change_mode (work->fro->fd, newworkmode)) ? 0
    : chmod (mani_filename, newworkmode)
    ;
}

static int
xpandfile (struct work *work, struct delta const *delta,
           char const **exname, bool dolog)
/* Read ‘work->fro’ and copy it to a file, performing keyword
   substitution with data from ‘delta’.
   Return -1 if unsuccessful, 1 if expansion occurred, 0 otherwise.
   If successful, store the name into ‘*exname’.  */
{
  char const *targetname;
  int e, r;

  targetname = makedirtemp (true);
  if (!(work->ex = fopen_safer (targetname, FOPEN_W_WORK)))
    {
      syserror_errno (targetname);
      MERR ("can't build working file");
      return -1;
    }
  r = 0;
  if (MIN_UNEXPAND <= BE (kws))
    fro_spew (work->fro, work->ex);
  else
    {
      struct expctx ctx = EXPCTX_1OUT (work->ex, work->fro, false, dolog);

      for (;;)
        {
          e = expandline (&ctx);
          if (e < 0)
            break;
          r |= e;
          if (e <= 1)
            break;
        }
      FINISH_EXPCTX (&ctx);
    }
  *exname = targetname;
  return ODDP (r);
}

/* --------------------- G E T L O G M S G --------------------------------*/

#define FIRST  "Initial revision"

static struct cbuf
getlogmsg (struct reason *reason, struct bud *bud)
/* Obtain and return a log message.
   If a log message is given with ‘-m’, return that message.
   If this is the initial revision, return a standard log message.
   Otherwise, read a character string from the terminal.
   Stop after reading EOF or a single '.' on a line.
   Prompt the first time called for the log message; during all
   later calls ask whether the previous log message can be reused.  */
{
  const char *num;

  if (reason->upfront.size)
    return reason->upfront;

  if (bud->keep)
    {
      char datebuf[datesize + zonelenmax];

      /* Generate standard log message.  */
      date2str (getcurdate (bud), datebuf);
      ACCF ("%s%s at %s", TINYKS (ciklog), getcaller (), datebuf);
      OK (&reason->delayed);
      return reason->delayed;
    }

  if (!bud->target
      && bud->num.size
      && (num = bud->num.string)
      && (NUM_EQ (num, "1.1")
          || NUM_EQ (num, "1.0")))
    {
      struct cbuf const initiallog =
        {
          .string = FIRST,
          .size = sizeof (FIRST) - 1
        };

      return initiallog;
    }

  if (reason->delayed.size)
    {
      /*Previous log available.  */
      if (yesorno (true, "reuse log message of previous file? [yn](y): "))
        return reason->delayed;
    }

  /* Now read string from stdin.  */
  reason->delayed = getsstdin ("m", "log message", "");

  /* Now check whether the log message is not empty.  */
  if (!reason->delayed.size)
    set_empty_log_message (&reason->delayed);
  return reason->delayed;
}

static char const *
first_meaningful_symbolic_name (struct link *ls)
{
  struct u_symdef const *ud;

  /* Find last link so that, e.g., "-nA -nB -nC" yields "A".
     See: (search-forward "symbolic_names = prepend")  */
  while (ls && ls->next)
    ls = ls->next;

  ud = ls->entry;
  return ud->u.meaningful;
}

int
ci_main (const char *cmd, int argc, char **argv)
{
  int exitstatus = EXIT_SUCCESS;
  struct reason reason;
  char altdate[datesize];
  char olddate[datesize];
  char newdatebuf[datesize + zonelenmax];
  char targetdatebuf[datesize + zonelenmax];
  char *a, **newargv, *textfile;
  char const *author, *krev, *rev, *state;
  char const *diffname, *expname;
  char const *newworkname;
  struct work work = { .ex = NULL };
  bool forceciflag = false;
  bool keepworkingfile = false;
  bool rcsinitflag = false;
  bool initflag, mustread;
  bool lockflag, lockthis, mtimeflag;
  int removedlock;
  bool Ttimeflag;
  int r;
  int changedRCS, changework;
  bool dolog, newhead;
  bool usestatdate;             /* Use mod time of file for -d.  */
  mode_t newworkmode;           /* mode for working file */
  time_t mtime, wtime;
  struct bud bud;
  struct delta *workdelta;
  struct link *symbolic_names = NULL;
  struct wlink *deltas;                 /* Deltas to be generated.  */
  const struct program program =
    {
      .invoke = argv[0],
      .name = cmd,
      .help = ci_help,
      .tyag = BOG_FULL
    };

  CHECK_HV ();
  gnurcs_init (&program);

  /* This lameness is because constructing a proper initialization form for
     ‘struct bud’ is too much hassle.  We do it here, after the ‘gnurcs_init’
     instead of before, closer to the declaration (as would be more indicative
     of its role) because perhaps Real Soon Now But Not Quite Yet ‘bud’ will
     be changed to be heap-allocated (probably in ‘PLEXUS’), and this is the
     place to do that.  */
  memset (&bud, 0, sizeof (struct bud));
  /* Likewise.  */
  memset (&reason, 0, sizeof (reason));

  setrid ();

  author = rev = state = textfile = NULL;
  initflag = lockflag = mustread = false;
  mtimeflag = false;
  Ttimeflag = false;
  altdate[0] = '\0';            /* empty alternate date for -d */
  usestatdate = false;
  BE (pe) = X_DEFAULT;

  argc = getRCSINIT (argc, argv, &newargv);
  argv = newargv;
  while (a = *++argv, 0 < --argc && *a++ == '-')
    {
      switch (*a++)
        {

        case 'r':
          if (*a)
            goto revno;
          keepworkingfile = lockflag = false;
          break;

        case 'l':
          keepworkingfile = lockflag = true;
        revno:
          if (*a)
            {
              if (rev)
                PWARN ("redefinition of %s", ks_revno);
              rev = a;
            }
          break;

        case 'u':
          keepworkingfile = true;
          lockflag = false;
          goto revno;

        case 'i':
          initflag = true;
          goto revno;

        case 'j':
          mustread = true;
          goto revno;

        case 'I':
          BE (interactive) = true;
          goto revno;

        case 'q':
          BE (quiet) = true;
          goto revno;

        case 'f':
          forceciflag = true;
          goto revno;

        case 'k':
          bud.keep = true;
          goto revno;

        case 'm':
          if (reason.upfront.size)
            redefined ('m');
          reason.upfront = cleanlogmsg (a, strlen (a));
          if (!reason.upfront.size)
            set_empty_log_message (&reason.upfront);
          break;

        case 'n':
        case 'N':
          {
            char option = a[-1];
            struct u_symdef *ud;

            if (!*a)
              {
                PERR ("missing symbolic name after -%c", option);
                break;
              }
            checkssym (a);
            ud = ZLLOC (1, struct u_symdef);
            ud->override = ('N' == option);
            ud->u.meaningful = a;
            symbolic_names = prepend (ud, symbolic_names, PLEXUS);
          }
          break;

        case 's':
          if (*a)
            {
              if (state)
                redefined ('s');
              checksid (a);
              state = a;
            }
          else
            PERR ("missing state for -s option");
          break;

        case 't':
          if (*a)
            {
              if (textfile)
                redefined ('t');
              textfile = a;
            }
          break;

        case 'd':
          if (altdate[0] || usestatdate)
            redefined ('d');
          altdate[0] = '\0';
          if (!(usestatdate = !*a))
            str2date (a, altdate);
          break;

        case 'M':
          mtimeflag = true;
          goto revno;

        case 'w':
          if (*a)
            {
              if (author)
                redefined ('w');
              checksid (a);
              author = a;
            }
          else
            PERR ("missing author for -w option");
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

        case 'T':
          if (!*a)
            {
              Ttimeflag = true;
              break;
            }
          /* fall into */
        default:
          bad_option (*argv);
        };
    }
  /* (End processing of options.)  */

  /* Handle all filenames.  */
  if (FLOW (erroneousp))
    cleanup (&exitstatus, &work);
  else if (argc < 1)
    PFATAL ("no input file");
  else
    for (; 0 < argc; cleanup (&exitstatus, &work), ++argv, --argc)
      {
        /* Use var instead of simple #define for fast identity compare.  */
        char const *default_state = DEFAULTSTATE;
        char const *mani_filename, *pv;
        struct fro *from;
        struct stat *repo_stat;
        FILE *frew;
        struct delta *tip;
        int kws;
        struct cbuf newdesc =
          {
            .string = NULL,
            .size = 0
          };

        bud.target = NULL;
        ffree ();

        switch (pairnames (argc, argv, rcswriteopen, mustread, false))
          {

          case -1:
            /* New RCS file.  */
            if (currently_setuid_p ())
              {
                MERR
                  ("setuid initial checkin prohibited; use `rcs -i -a' first");
                continue;
              }
            rcsinitflag = true;
            break;

          case 0:
            /* Error.  */
            continue;

          case 1:
            /* Normal checkin with previous RCS file.  */
            if (initflag)
              {
                RERR ("already exists");
                continue;
              }
            rcsinitflag = !(tip = REPO (tip));
          }

        /* ‘REPO (filename)’ contains the name of the RCS file,
           and ‘MANI (filename)’ contains the name of the working file.
           If the RCS file exists, ‘FLOW (from)’ contains the file
           descriptor for the RCS file, and ‘REPO (stat)’ is set.
           The admin node is initialized.  */
        mani_filename = MANI (filename);
        from = FLOW (from);
        repo_stat = &REPO (stat);
        kws = BE (kws);

        diagnose ("%s  <--  %s", REPO (filename), mani_filename);

        if (!(work.fro = fro_open (mani_filename, FOPEN_R_WORK, &work.st)))
          {
            syserror_errno (mani_filename);
            continue;
          }

        if (from)
          {
            if (SAME_INODE (REPO (stat), work.st))
              {
                RERR ("RCS file is the same as working file %s.",
                      mani_filename);
                continue;
              }
            if (!checkaccesslist ())
              continue;
          }

        krev = rev;
        if (bud.keep)
          {
            /* Get keyword values from working file.  */
            if (!getoldkeys (work.fro))
              continue;
            if (!rev && !(krev = PREV (rev)))
              {
                MERR ("can't find a %s", ks_revno);
                continue;
              }
            if (!PREV (date) && *altdate == '\0' && usestatdate == false)
              MWARN ("can't find a date");
            if (!PREV (author) && !author)
              MWARN ("can't find an author");
            if (!PREV (state) && !state)
              MWARN ("can't find a state");
          }

        /* Expand symbolic revision number.  */
        if (!fully_numeric (&bud.num, krev, work.fro))
          continue;

        /* Splice new delta into tree.  */
        if (PROB (removedlock = addelta (&deltas, &bud, rcsinitflag)))
          continue;
        tip = REPO (tip);

        bud.d.num = bud.num.string;
        bud.d.branches = NULL;
        /* This might be changed by ‘addlock’.  */
        bud.d.lockedby = NULL;
        bud.d.selector = true;
        bud.d.name = NULL;

        /* Set author.  */
        if (author)
          /* Given by ‘-w’.  */
          bud.d.author = author;
        else if (bud.keep && (pv = PREV (author)))
            /* Preserve old author if possible.  */
          bud.d.author = pv;
        else
          /* Otherwise use caller's id.  */
          bud.d.author = getcaller ();

        /* Set state.  */
        bud.d.state = default_state;
        if (state)
          /* Given by ‘-s’.  */
          bud.d.state = state;
        else if (bud.keep && (pv = PREV (state)))
          /* Preserve old state if possible.  */
          bud.d.state = pv;

        /* Compute date.  */
        if (usestatdate)
          {
            time2date (work.st.st_mtime, altdate);
          }
        if (*altdate != '\0')
          /* Given by ‘-d’.  */
          bud.d.date = altdate;
        else if (bud.keep && (pv = PREV (date)))
          {
            /* Preserve old date if possible.  */
            str2date (pv, olddate);
            bud.d.date = olddate;
          }
        else
          /* Use current date.  */
          bud.d.date = getcurdate (&bud);
        /* Now check validity of date -- needed because of ‘-d’ and ‘-k’.  */
        if (bud.target && DATE_LT (bud.d.date, bud.target->date))
          {
            RERR ("Date %s precedes %s in revision %s.",
                  date2str (bud.d.date, newdatebuf),
                  date2str (bud.target->date, targetdatebuf),
                  bud.target->num);
            continue;
          }

        if (lockflag && addlock (&bud.d, true) < 0)
          continue;

        if (bud.keep && (pv = PREV (name)))
          if (addsymbol (bud.d.num, pv, false) < 0)
            continue;
        if (!addsyms (bud.d.num, symbolic_names))
          continue;

        putadmin ();
        frew = FLOW (rewr);
        puttree (tip, frew);
        putdesc (&newdesc, false, textfile);

        changework = kws < MIN_UNCHANGED_EXPAND;
        dolog = true;
        lockthis = lockflag;
        workdelta = &bud.d;

        /* Build rest of file.  */
        if (rcsinitflag)
          {
            diagnose ("initial revision: %s", bud.d.num);
            /* Get logmessage.  */
            bud.d.pretty_log = getlogmsg (&reason, &bud);
            putdftext (&bud.d, work.fro, frew, false);
            repo_stat->st_mode = work.st.st_mode;
            repo_stat->st_nlink = 0;
            changedRCS = true;
            if (from)
              IGNORE_REST (from);
          }
        else
          {
            diffname = maketemp (0);
            newhead = tip == &bud.d;
            if (!newhead)
              FLOW (to) = frew;
            expname = buildrevision (deltas, bud.target, NULL, false);
            if (!forceciflag
                && STR_SAME (bud.d.state, bud.target->state)
                && ((changework = rcsfcmp (work.fro, &work.st, expname,
                                           bud.target))
                    <= 0))
              {
                diagnose
                  ("file is unchanged; reverting to previous revision %s",
                   bud.target->num);
                if (removedlock < lockflag)
                  {
                    diagnose
                      ("previous revision was not locked; ignoring -l option");
                    lockthis = 0;
                  }
                dolog = false;
                if (!(changedRCS = lockflag < removedlock || symbolic_names))
                  {
                    workdelta = bud.target;
                    SAME_AFTER (from, bud.target->text);
                  }
                else
                  /* We have started to build the wrong new RCS file.
                     Start over from the beginning.  */
                  {
                    off_t hwm = ftello (frew);
                    bool bad_truncate;

                    rewind (frew);
                    bad_truncate = PROB (ftruncate (fileno (frew), (off_t) 0));
                    grok_resynch (REPO (r));

                    /* The ‘bud.d’ might still be linked in the tree,
                       so prune it now.  (Unfortunately, ‘grok_resynch’
                       did not restore the tree completely, as its name
                       might imply.)  */
                    prune (&bud.d, bud.target);

                    if (! (workdelta = gr_revno (bud.target->num, &deltas)))
                      continue;
                    workdelta->pretty_log = bud.target->pretty_log;
                    if (bud.d.state != default_state)
                      workdelta->state = bud.d.state;
                    if (lockthis < removedlock && removelock (workdelta) < 0)
                      continue;
                    if (!addsyms (workdelta->num, symbolic_names))
                      continue;
                    if (PROB (dorewrite (true, true)))
                      continue;
                    VERBATIM (from, GROK (neck));
                    fro_spew (from, frew);
                    if (bad_truncate)
                      while (ftello (frew) < hwm)
                        /* White out any earlier mistake with '\n's.
                           This is unlikely.  */
                        afputc ('\n', frew);
                  }
              }
            else
              {
                int wfd = work.fro->fd;
                struct stat checkworkstat;
                char const *diffv[6 + !!OPEN_O_BINARY], **diffp;

                diagnose ("new revision: %s; previous revision: %s",
                          bud.d.num, bud.target->num);
                SAME_AFTER (from, bud.target->text);
                bud.d.pretty_log = getlogmsg (&reason, &bud);

                /* "Rewind" ‘work.fro’ before feeding it to diff(1).  */
                fro_bob (work.fro);
                if (PROB (lseek (wfd, 0, SEEK_SET)))
                  Ierror ();

                diffp = diffv;
                *++diffp = prog_diff;
                *++diffp = diff_flags;
#if OPEN_O_BINARY
                if (kws == kwsub_b)
                  *++diffp = "--binary";
#endif
                *++diffp = newhead ? "-" : expname;
                *++diffp = newhead ? expname : "-";
                *++diffp = NULL;
                if (DIFF_TROUBLE == runv (wfd, diffname, diffv))
                  RFATAL ("diff failed");
                if (newhead)
                  {
                    fro_bob (work.fro);
                    putdftext (&bud.d, work.fro, frew, false);
                    if (!putdtext (bud.target, diffname, frew, true))
                      continue;
                  }
                else if (!putdtext (&bud.d, diffname, frew, true))
                  continue;

                /* Check whether the working file changed during checkin,
                   to avoid producing an inconsistent RCS file.  */
                if (PROB (fstat (wfd, &checkworkstat))
                    || work.st.st_mtime != checkworkstat.st_mtime
                    || work.st.st_size != checkworkstat.st_size)
                  {
                    MERR ("file changed during checkin");
                    continue;
                  }

                changedRCS = true;
              }
          }

        /* Deduce ‘time_t’ of new revision if it is needed later.  */
        wtime = (time_t) - 1;
        if (mtimeflag | Ttimeflag)
          wtime = date2time (workdelta->date);

        if (PROB (donerewrite (changedRCS, !Ttimeflag
                               ? (time_t) - 1
                               : ((from && wtime < repo_stat->st_mtime)
                                  ? repo_stat->st_mtime
                                  : wtime))))
          continue;

        if (!keepworkingfile)
          {
            fro_zclose (&work.fro);
            /* Get rid of old file.  */
            r = un_link (mani_filename);
          }
        else
          {
            newworkmode = WORKMODE (repo_stat->st_mode,
                                    !(kws == kwsub_v
                                      || lockthis < BE (strictly_locking)));
            mtime = mtimeflag ? wtime : (time_t) - 1;

            /* Expand if it might change or if we can't fix mode, time.  */
            if (changework || PROB (r = fixwork (newworkmode, mtime, &work)))
              {
                fro_bob (work.fro);
                /* Expand keywords in file.  */
                BE (inclusive_of_Locker_in_Id_val) = lockthis;
                workdelta->name =
                  namedrev (symbolic_names
                            ? first_meaningful_symbolic_name (symbolic_names)
                            : (bud.keep && (pv = PREV (name))
                               ? pv
                               : rev),
                            workdelta);
                switch (xpandfile (&work, workdelta, &newworkname, dolog))
                  {
                  default:
                    continue;

                  case 0:
                    /* No expansion occurred; try to reuse working file
                       unless we already tried and failed.  */
                    if (changework)
                      if ((r = fixwork (newworkmode, mtime, &work)) == 0)
                        break;
                    /* fall into */
                  case 1:
                    fro_zclose (&work.fro);
                    aflush (work.ex);
                    IGNOREINTS ();
                    r = chnamemod (&work.ex, newworkname, mani_filename,
                                   1, newworkmode, mtime);
                    keepdirtemp (newworkname);
                    RESTOREINTS ();
                  }
              }
          }
        if (PROB (r))
          {
            syserror_errno (mani_filename);
            continue;
          }
        diagnose ("done");

      }

  tempunlink ();
  gnurcs_goodbye ();
  return exitstatus;
}

const uint8_t ci_aka[19] =
{
  3 /* count */,
  2,'c','i',
  7,'c','h','e','c','k','i','n',
  6,'c','o','m','m','i','t'
};

/*:help
[options] file...
Options:
  -f[REV]       Force new entry, even if no content changed.
  -I[REV]       Interactive.
  -i[REV]       Initial checkin; error if RCS file already exists.
  -j[REV]       Just checkin, don't init; error if RCS file does not exist.
  -k[REV]       Compute revision from working file keywords.
  -q[REV]       Quiet mode.
  -r[REV]       Do normal checkin, if REV is specified;
                otherwise, release lock and delete working file.
  -l[REV]       Like -r, but immediately checkout locked (co -l) afterwards.
  -u[REV]       Like -l, but checkout unlocked (co -u).
  -M[REV]       Reset working file mtime (relevant for -l, -u).
  -d[DATE]      Use DATE (or working file mtime).
  -mMSG         Use MSG as the log message.
  -nNAME        Assign symbolic NAME to the entry; NAME must be new.
  -NNAME        Like -n, but overwrite any previous assignment.
  -sSTATE       Set state to STATE.
  -t-TEXT       Set description to TEXT.
  -tFILENAME    Set description from text read from FILENAME.
  -T            Set the RCS file's modification time to the new
                revision's time if the former precedes the latter and there
                is a new revision; preserve the RCS file's modification
                time otherwise.
  -V            Like --version.
  -VN           Emulate RCS version N.
  -wWHO         Use WHO as the author.
  -xSUFF        Specify SUFF as a slash-separated list of suffixes
                used to identify RCS file names.
  -zZONE        Specify date output format in keyword-substitution
                and also the default timezone for -dDATE.

Multiple flags in {fiIjklMqru} may be used, except for -r, -l, -u, which are
mutually exclusive.  If specified, REV can be symbolic, numeric, or mixed:
  symbolic      Must have been defined previously (see -n, -N).
  $             Determine from keyword values in the working file.
  .N            Prepend default branch => DEFBR.N
  BR.N          Use this, but N must be greater than any existing
                on BR, or BR must be new.
  BR            Latest rev on branch BR + 1 => BR.(L+1), or BR.1 if new branch.
If REV is omitted, compute it from the last lock (co -l), perhaps
starting a new branch.  If there is no lock, use DEFBR.(L+1).
*/

/* ci.c ends here */

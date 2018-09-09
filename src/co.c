/* Check out working files from revisions of RCS files.

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
#include <stdlib.h>
#include "same-inode.h"
#include "co.help"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-esds.h"
#include "b-excwho.h"
#include "b-fb.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-isr.h"
#include "b-peer.h"

struct work
{
  struct stat st;
  bool force;
};

static char const quietarg[] = "-q";

/* State for -j.  */
struct jstuff
{
  struct divvy *jstuff;
  struct link head, *tp;
  struct symdef *merge;
  char const *expand, *suffix, *version, *zone;

  struct delta *d;
  /* Final delta to be generated.  */

  char const **ls;
  /* Revisions to be joined.  */

  int lastidx;
  /* Index of last element in `ls'.  */
};

static void
cleanup (int *exitstatus, FILE **neworkptr)
{
  FILE *mstdout = MANI (standard_output);

  if (FLOW (erroneousp))
    *exitstatus = EXIT_FAILURE;
  fro_zclose (&FLOW (from));
  ORCSclose ();
  if (FLOW (from)
      && STDIO_P (FLOW (from))
      && FLOW (res)
      && FLOW (res) != mstdout)
    Ozclose (&FLOW (res));
  if (*neworkptr != mstdout)
    Ozclose (neworkptr);
  dirtempunlink ();
}

static bool
rmworkfile (struct work *work)
/* Prepare to remove ‘MANI (filename)’, if it exists, and if it is read-only.
   Otherwise (file writable), if !quietmode, ask the user whether to
   really delete it (default: fail); otherwise fail.
   Return true if permission is gotten.  */
{
  if (work->st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH) && !work->force)
    {
      char const *mani_filename = MANI (filename);

      /* File is writable.  */
      if (!yesorno (false, "writable %s exists%s; remove it? [ny](n): ",
                    mani_filename, (stat_mine_p (&work->st)
                                    ? ""
                                    : ", and you do not own it")))
        {
          if (!BE (quiet) && ttystdin ())
            PERR ("checkout aborted");
          else
            PERR ("writable %s exists; checkout aborted", mani_filename);
          return false;
        }
    }
  /* Actual unlink is done later by caller.  */
  return true;
}

static int
rmlock (struct delta const *delta)
/* Remove the lock held by caller on ‘delta’.  Return -1 if
  someone else holds the lock, 0 if there is no lock on delta,
  and 1 if a lock was found and removed.  */
{
  struct link box, *tp;
  struct rcslock const *rl;

  box.next = GROK (locks);
  if (! (tp = lock_delta_memq (&box, delta)))
    /* No lock on ‘delta’.  */
    return 0;
  rl = tp->next->entry;
  if (!caller_login_p (rl->login))
    /* Found a lock on ‘delta’ by someone else.  */
    {
      RERR ("revision %s locked by %s; use co -r or rcs -u",
            delta->num, rl->login);
      return -1;
    }
  /* Found a lock on ‘delta’ by caller; delete it.  */
  lock_drop (&box, tp);
  return 1;
}

static void
jpush (char const *rev, struct jstuff *js)
{
  js->tp = extend (js->tp, rev, js->jstuff);
  js->lastidx++;
}

static char *
addjoin (char *joinrev, struct jstuff *js)
/* Add the number of ‘joinrev’ to ‘js->ls’; return address
   of char past ‘joinrev’, or NULL if no such revision exists.  */
{
  register char *j;
  register struct delta *d;
  char terminator;
  struct cbuf numrev;

  j = joinrev;
  for (;;)
    {
      switch (*j++)
        {
        default:
          continue;
        case 0:
        case ' ':
        case '\t':
        case '\n':
        case ':':
        case ',':
        case ';':
          break;
        }
      break;
    }
  terminator = *--j;
  *j = '\0';
  d = NULL;
  if (fully_numeric_no_k (&numrev, joinrev))
    d = delta_from_ref (numrev.string);
  *j = terminator;
  if (d)
    {
      jpush (d->num, js);
      return j;
    }
  return NULL;
}

static char const *
getancestor (char const *r1, char const *r2)
/* Return the common ancestor of ‘r1’ and ‘r2’ if successful,
   ‘NULL’ otherwise.  Work reliably only if ‘r1’ and ‘r2’ are not
   branch numbers.   */
{
  char const *t1, *t2;
  int l1, l2, l3;
  char const *r;

  /* TODO: Don't bother saving in ‘PLEXUS’.  */

  l1 = countnumflds (r1);
  l2 = countnumflds (r2);
  if ((2 < l1 || 2 < l2) && !NUM_EQ (r1, r2))
    {
      /* Not on main trunk or identical.  */
      l3 = 0;
      while (NUMF_EQ (1 + l3, r1, r2)
             && NUMF_EQ (2 + l3, r1, r2))
        l3 += 2;
      /* This will terminate since ‘r1’ and ‘r2’ are not the
         same; see above.  */
      if (l3 == 0)
        {
          /* No common prefix; common ancestor on main trunk.  */
          t1 = TAKE (l1 > 2 ? 2 : l1, r1);
          t2 = TAKE (l2 > 2 ? 2 : l2, r2);
          r = NUM_LT (t1, t2) ? t1 : t2;
          if (!NUM_EQ (r, r1) && !NUM_EQ (r, r2))
            return str_save (r);
        }
      else if (!NUMF_EQ (1 + l3, r1, r2))
        return str_save (TAKE (l3, r1));
    }
  RERR ("common ancestor of %s and %s undefined", r1, r2);
  return NULL;
}

static bool
preparejoin (register char *j, struct jstuff *js)
/* Parse join list ‘j’ and place pointers to the
   revision numbers into ‘js->ls’.
   Set ‘js->lastidx’ to the last index of the list.  */
{
  bool rv = true;

  js->jstuff = make_space ("jstuff");
  js->head.next = NULL;
  js->tp = &js->head;
  if (! js->merge)
    {
      js->merge = ZLLOC (1, struct symdef);
      js->merge->meaningful = "merge";
    }

  js->lastidx = -1;
  for (;;)
    {
      while ((*j == ' ') || (*j == '\t') || (*j == ','))
        j++;
      if (*j == '\0')
        break;
      if (!(j = addjoin (j, js)))
        return false;
      while ((*j == ' ') || (*j == '\t'))
        j++;
      if (*j == ':')
        {
          j++;
          while ((*j == ' ') || (*j == '\t'))
            j++;
          if (*j != '\0')
            {
              if (!(j = addjoin (j, js)))
                return false;
            }
          else
            {
              RFATAL ("join pair incomplete");
            }
        }
      else
        {
          if (js->lastidx == 0)         /* first pair */
            {
              char const *two = js->tp->entry;

              /* Derive common ancestor.  */
              if (! (js->tp->entry = getancestor (js->d->num, two)))
                {
                  rv = false;
                  goto done;
                }
              /* Common ancestor missing.  */
              jpush (two, js);
            }
          else
            {
              RFATAL ("join pair incomplete");
            }
        }
    }
  if (js->lastidx < 1)
    RFATAL ("empty join");
 done:

  js->ls = pointer_array (PLEXUS, 1 + js->lastidx);
  js->tp = js->head.next;
  for (int i = 0; i <= js->lastidx; i++, js->tp = js->tp->next)
    js->ls[i] = js->tp->entry;
  close_space (js->jstuff);
  js->jstuff = NULL;
  return rv;
}

/* Elements in the constructed command line prior to this index are
   boilerplate.  From this index on, things are data-dependent.  */
#define VX  3

static bool
buildjoin (char const *initialfile, struct jstuff *js)
/* Merge pairs of elements in ‘js->ls’ into ‘initialfile’.
   If ‘MANI (standard_output)’ is set, copy result to stdout.
   All unlinking of ‘initialfile’, ‘rev2’, and ‘rev3’
   should be done by ‘tempunlink’.  */
{
  char const *rev2, *rev3;
  int i;
  char const *cov[8 + VX], *mergev[11];
  char const **p;
  size_t len;
  char const *subs = NULL;

  rev2 = maketemp (0);
  rev3 = maketemp (3);      /* ‘buildrevision’ may use 1 and 2 */

  cov[1] = PEER_SUPER ();
  cov[2] = "co";
  /* ‘cov[VX]’ setup below.  */
  p = &cov[1 + VX];
  if (js->expand)
    *p++ = js->expand;
  if (js->suffix)
    *p++ = js->suffix;
  if (js->version)
    *p++ = js->version;
  if (js->zone)
    *p++ = js->zone;
  *p++ = quietarg;
  *p++ = REPO (filename);
  *p = '\0';

  mergev[1] = find_peer_prog (js->merge);
  mergev[2] = mergev[4] = "-L";
  /* Rest of ‘mergev’ setup below.  */

  i = 0;
  while (i < js->lastidx)
    {
#define ACCF(...)  accf (SINGLE, __VA_ARGS__)
      /* Prepare marker for merge.  */
      if (i == 0)
        subs = js->d->num;
      else
        {
          ACCF ("%s,%s:%s", subs, js->ls[i - 2], js->ls[i - 1]);
          subs = finish_string (SINGLE, &len);
        }
      diagnose ("revision %s", js->ls[i]);
      ACCF ("-p%s", js->ls[i]);
      cov[VX] = finish_string (SINGLE, &len);
      if (runv (-1, rev2, cov))
        goto badmerge;
      diagnose ("revision %s", js->ls[i + 1]);
      ACCF ("-p%s", js->ls[i + 1]);
      cov[VX] = finish_string (SINGLE, &len);
      if (runv (-1, rev3, cov))
        goto badmerge;
      diagnose ("merging...");
      mergev[3] = subs;
      mergev[5] = js->ls[i + 1];
      p = &mergev[6];
      if (BE (quiet))
        *p++ = quietarg;
      if (js->lastidx <= i + 2 && MANI (standard_output))
        *p++ = "-p";
      *p++ = initialfile;
      *p++ = rev2;
      *p++ = rev3;
      *p = '\0';
      if (DIFF_TROUBLE == runv (-1, NULL, mergev))
          goto badmerge;
      i = i + 2;
#undef ACCF
    }
  return true;

badmerge:
  FLOW (erroneousp) = true;
  return false;
}

int
co_main (const char *cmd, int argc, char **argv)
{
  int exitstatus = EXIT_SUCCESS;
  struct work work = { .force = false };
  struct jstuff jstuff;
  FILE *neworkptr = NULL;
  int lockflag = 0;                 /* -1: unlock, 0: do nothing, 1: lock.  */
  bool mtimeflag = false;
  char *a, *joinflag, **newargv;
  char const *author, *date, *rev, *state;
  char const *joinname, *newdate, *neworkname;
  /* 1 if a lock has been changed, -1 if error.  */
  int changelock;
  int expmode, r, workstatstat;
  bool tostdout, Ttimeflag, selfsame;
  char finaldate[datesize];
#if OPEN_O_BINARY
  int stdout_mode = 0;
#endif
  struct wlink *deltas;                 /* Deltas to be generated.  */
  const struct program program =
    {
      .invoke = argv[0],
      .name = cmd,
      .help = co_help,
      .tyag = BOG_FULL
    };

  CHECK_HV ();
  gnurcs_init (&program);
  memset (&jstuff, 0, sizeof (struct jstuff));

  setrid ();
  author = date = rev = state = NULL;
  joinflag = NULL;
  expmode = -1;
  BE (pe) = X_DEFAULT;
  tostdout = false;
  Ttimeflag = false;
  selfsame = false;

  argc = getRCSINIT (argc, argv, &newargv);
  argv = newargv;
  while (a = *++argv, 0 < --argc && *a++ == '-')
    {
      switch (*a++)
        {

        case 'r':
        revno:
          if (*a)
            {
              if (rev)
                PWARN ("redefinition of %s", ks_revno);
              rev = a;
            }
          break;

        case 'f':
          work.force = true;
          goto revno;

        case 'l':
          if (lockflag < 0)
            {
              PWARN ("-u overridden by -l.");
            }
          lockflag = 1;
          goto revno;

        case 'u':
          if (0 < lockflag)
            {
              PWARN ("-l overridden by -u.");
            }
          lockflag = -1;
          goto revno;

        case 'p':
          tostdout = true;
          goto revno;

        case 'I':
          BE (interactive) = true;
          goto revno;

        case 'q':
          BE (quiet) = true;
          goto revno;

        case 'd':
          if (date)
            redefined ('d');
          str2date (a, finaldate);
          date = finaldate;
          break;

        case 'j':
          if (*a)
            {
              if (joinflag)
                redefined ('j');
              joinflag = a;
            }
          break;

        case 'M':
          mtimeflag = true;
          goto revno;

        case 's':
          if (*a)
            {
              if (state)
                redefined ('s');
              state = a;
            }
          break;

        case 'S':
          selfsame = true;
          break;

        case 'T':
          if (*a)
            goto unknown;
          Ttimeflag = true;
          break;

        case 'w':
          if (author)
            redefined ('w');
          if (*a)
            author = a;
          else
            author = getcaller ();
          break;

        case 'x':
          jstuff.suffix = *argv;
          BE (pe) = a;
          break;

        case 'V':
          jstuff.version = *argv;
          setRCSversion (jstuff.version);
          break;

        case 'z':
          jstuff.zone = *argv;
          zone_set (a);
          break;

        case 'k':
          /* Set keyword expand mode.  */
          jstuff.expand = *argv;
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
  /* (End of option processing.)  */

  /* Now handle all filenames.  */
  if (FLOW (erroneousp))
    cleanup (&exitstatus, &neworkptr);
  else if (argc < 1)
    PFATAL ("no input file");
  else
    for (; 0 < argc; cleanup (&exitstatus, &neworkptr), ++argv, --argc)
      {
        struct stat *repo_stat;
        char const *mani_filename;
        int kws;

        ffree ();

        if (pairnames
            (argc, argv, lockflag ? rcswriteopen : rcsreadopen, true,
             false) <= 0)
          continue;

        /* ‘REPO (filename)’ contains the name of the RCS file, and
           ‘FLOW (from)’ points at it.  ‘MANI (filename)’ contains the
           name of the working file.  Also, ‘REPO (stat)’ has been set.  */
        repo_stat = &REPO (stat);
        mani_filename = MANI (filename);
        kws = BE (kws);
        diagnose ("%s  -->  %s", REPO (filename),
                  tostdout ? "standard output" : mani_filename);

        workstatstat = -1;
        if (tostdout)
          {
#if OPEN_O_BINARY
            int newmode = kws == kwsub_b ? OPEN_O_BINARY : 0;
            if (stdout_mode != newmode)
              {
                stdout_mode = newmode;
                oflush ();
                setmode (STDOUT_FILENO, newmode);
              }
#endif
            neworkname = NULL;
            neworkptr = MANI (standard_output) = stdout;
          }
        else
          {
            workstatstat = stat (mani_filename, &work.st);
            if (!PROB (workstatstat) && SAME_INODE (REPO (stat), work.st))
              {
                RERR ("RCS file is the same as working file %s.",
                      mani_filename);
                continue;
              }
            neworkname = makedirtemp (true);
            if (!(neworkptr = fopen_safer (neworkname, FOPEN_W_WORK)))
              {
                if (errno == EACCES)
                  MERR ("permission denied on parent directory");
                else
                  syserror_errno (neworkname);
                continue;
              }
          }

        if (!REPO (tip))
          {
            /* No revisions; create empty file.  */
            diagnose ("no revisions present; generating empty revision 0.0");
            if (lockflag)
              PWARN ("no revisions, so nothing can be %slocked",
                     lockflag < 0 ? "un" : "");
            Ozclose (&FLOW (res));
            if (!PROB (workstatstat))
              if (!rmworkfile (&work))
                continue;
            changelock = 0;
            newdate = NULL;
          }
        else
          {
            struct cbuf numericrev;
            int locks = lockflag ? findlock (false, &jstuff.d) : 0;
            struct fro *from = FLOW (from);

            if (rev)
              {
                /* Expand symbolic revision number.  */
                if (!fully_numeric_no_k (&numericrev, rev))
                  continue;
              }
            else
              {
                switch (locks)
                  {
                  default:
                    continue;
                  case 0:
                    numericrev.string = GROK (branch) ? GROK (branch) : "";
                    break;
                  case 1:
                    numericrev.string = str_save (jstuff.d->num);
                    break;
                  }
              }
            /* Get numbers of deltas to be generated.  */
            if (! (jstuff.d = genrevs (numericrev.string, date, author,
                                          state, &deltas)))
              continue;
            /* Check reservations.  */
            changelock = lockflag < 0
              ? rmlock (jstuff.d)
              : lockflag == 0 ? 0 : addlock_maybe (jstuff.d, selfsame, true);

            if (changelock < 0
                || (changelock && !checkaccesslist ())
                || PROB (dorewrite (lockflag, changelock)))
              continue;

            if (0 <= expmode)
              kws = BE (kws) = expmode;
            if (0 < lockflag && kws == kwsub_v)
              {
                RERR ("cannot combine -kv and -l");
                continue;
              }

            if (joinflag && !preparejoin (joinflag, &jstuff))
              continue;

            diagnose ("revision %s%s", jstuff.d->num,
                      0 < lockflag ? " (locked)" :
                      lockflag < 0 ? " (unlocked)" : "");
            SAME_AFTER (from, jstuff.d->text);

            /* Prepare to remove old working file if necessary.  */
            if (!PROB (workstatstat))
              if (!rmworkfile (&work))
                continue;

            /* Skip description (don't echo).  */
            write_desc_maybe (FLOW (to));

            BE (inclusive_of_Locker_in_Id_val) = 0 < lockflag;
            jstuff.d->name = namedrev (rev, jstuff.d);
            joinname = buildrevision (deltas, jstuff.d,
                                      joinflag && tostdout ? NULL : neworkptr,
                                      kws < MIN_UNEXPAND);
            if (FLOW (res) == neworkptr)
              FLOW (res) = NULL;             /* Don't close it twice.  */
            if (changelock && deltas->entry != jstuff.d)
              fro_trundling (true, from);

            if (PROB (donerewrite (changelock, Ttimeflag
                                   ? repo_stat->st_mtime
                                   : (time_t) - 1)))
              continue;

            if (changelock)
              {
                locks += lockflag;
                if (1 < locks)
                  RWARN ("You now have %d locks.", locks);
              }

            newdate = jstuff.d->date;
            if (joinflag)
              {
                newdate = NULL;
                if (!joinname)
                  {
                    aflush (neworkptr);
                    joinname = neworkname;
                  }
                if (kws == kwsub_b)
                  MERR ("merging binary files");
                if (!buildjoin (joinname, &jstuff))
                  continue;
              }
          }
        if (!tostdout)
          {
            mode_t m = WORKMODE (repo_stat->st_mode,
                                 !(kws == kwsub_v
                                   || (lockflag <= 0 && BE (strictly_locking))));
            time_t t = mtimeflag
              && newdate ? date2time (newdate) : (time_t) - 1;
            aflush (neworkptr);
            IGNOREINTS ();
            r = chnamemod (&neworkptr, neworkname, mani_filename, 1, m, t);
            keepdirtemp (neworkname);
            RESTOREINTS ();
            if (PROB (r))
              {
                syserror_errno (mani_filename);
                PERR ("see %s", neworkname);
                continue;
              }
            diagnose ("done");
          }
      }

  tempunlink ();
  Ozclose (&MANI (standard_output));
  gnurcs_goodbye ();
  return exitstatus;
}

const uint8_t co_aka[13] =
{
  2 /* count */,
  2,'c','o',
  8,'c','h','e','c','k','o','u','t'
};

/*:help
[options] file ...
Options:
  -f[REV]       Force overwrite of working file.
  -I[REV]       Interactive.
  -p[REV]       Write to stdout instead of the working file.
  -q[REV]       Quiet mode.
  -r[REV]       Normal checkout.
  -l[REV]       Like -r, but also lock.
  -u[REV]       Like -l, but unlock.
  -M[REV]       Reset working file mtime (relevant for -l, -u).
  -kSUBST       Use SUBST substitution, one of: kv, kvl, k, o, b, v.
  -dDATE        Select latest before or on DATE.
  -jJOINS       Merge using JOINS, a list of REV:REV pairs;
                this option is obsolete -- see rcsmerge(1).
  -sSTATE       Select matching state STATE.
  -S            Enable "self-same" mode.
  -T            Preserve the modification time on the RCS file
                even if it changes because a lock is added or removed.
  -wWHO         Select matching login WHO.
  -V            Like --version.
  -VN           Emulate RCS version N.
  -xSUFF        Specify SUFF as a slash-separated list of suffixes
                used to identify RCS file names.
  -zZONE        Specify date output format in keyword-substitution
                and also the default timezone for -dDATE.

Multiple flags in {fIlMpqru} may be used, except for -r, -l, -u, which are
mutually exclusive.  If specified, REV can be symbolic, numeric, or mixed:
  symbolic -- must have been defined previously (see ci(1))
  $        -- determine the revision number from keyword values
              in the working file
  .N       -- prepend default branch => DEFBR.N
  BR.N     -- use this
  BR       -- latest revision on branch BR
If REV is omitted, take it to be the latest on the default branch.
*/

/* co.c ends here */

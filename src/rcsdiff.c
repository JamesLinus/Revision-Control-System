/* Compare RCS revisions.

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
#include "rcsdiff.help"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-peer.h"

/* Normally, if the two revisions specified are the same, we avoid calling
   the underlying diff on the theory that it will produce no output.  This
   does not hold for -y (--side-by-side) and -D (--ifdef), however, such
   as when the revision specified is by different symbolic names, so we
   need to detect those options and disable the optimization.

   The ‘s_unique’, ‘minus_y’ ‘minus_D’, and ‘longopt_maybe_p’ are for
   detecting the long variants in a GNU getopt_long(3)-compatible way.  */

struct unique
{
  /* Can this option take a value preceded by '=' (--OPT=VAL)?  */
  bool eqval_p;
  /* Minimum length of bytes that must match (including "--").  */
  size_t minlen;
  /* The full longopt name (including "--").  */
  char const full[];
};
typedef struct unique s_unique;

static const s_unique const minus_y =
  {
    .eqval_p = false,
    .minlen = 4,
    .full = "--side-by-side"
  };
static const s_unique const minus_D =
  {
    .eqval_p = true,
    .minlen = 4,
    .full = "--ifdef"
  };

struct work
{
  struct stat st;
  struct fro *fro;
};

static inline bool
longopt_maybe_p (const char *arg, const s_unique const *u)
{
  const char *equal = u->eqval_p
    ? strchr (arg, '=')
    : NULL;
  size_t len = equal
    ? (size_t)(equal - arg)
    : strlen (arg);

  return !(u->minlen > len)
    && (0 == strncmp (arg, u->full, len));
}

static void
cleanup (int *exitstatus, struct work *work)
{
  if (FLOW (erroneousp))
    *exitstatus = DIFF_TROUBLE;
  fro_zclose (&FLOW (from));
  fro_zclose (&work->fro);
}

#if DIFF_L
static char const *
setup_label (char const *num, char const date[datesize])
{
  size_t len;
  char datestr[datesize + zonelenmax];

  date2str (date, datestr);
  accf (PLEXUS, "--label=%s\t%s", MANI (filename), datestr);
  if (num)
    accf (PLEXUS, "\t%s", num);
  return finish_string (PLEXUS, &len);
}
#endif

/* Elements in the constructed command line prior to this index are
   boilerplate.  From this index on, things are data-dependent.  */
#define COMMAND_LINE_VARYING  (4 + !DIFF_L)

int
rcsdiff_main (const char *cmd, int argc, char **argv)
{
  int exitstatus = DIFF_SUCCESS;
  struct work work;
  int revnums;                  /* counter for revision numbers given */
  char const *rev1, *rev2;      /* revision numbers from command line */
  char const *xrev1, *xrev2;    /* expanded revision numbers */
  char const *expandarg, *lexpandarg, *suffixarg, *versionarg, *zonearg;
#if DIFF_L
  int file_labels;
  char const **diff_label1, **diff_label2;
  char date2[datesize];
#endif
  char const *cov[7 + COMMAND_LINE_VARYING];
  char const **diffv, **diffp, **diffpend;      /* argv for subsidiary diff */
  char const **pp, *diffvstr = NULL;
  struct cbuf commarg;
  struct delta *target;
  char *a, *dcp, **newargv;
  bool no_diff_means_no_output;
  register int c;
  const struct program program =
    {
      .invoke = argv[0],
      .name = cmd,
      .help = rcsdiff_help,
      .tyag = BOG_DIFF
    };

  CHECK_HV ();
  gnurcs_init (&program);
  memset (&work, 0, sizeof (work));

  exitstatus = DIFF_SUCCESS;

  revnums = 0;
  rev1 = rev2 = xrev2 = NULL;
#if DIFF_L
  file_labels = 0;
#endif
  expandarg = suffixarg = versionarg = zonearg = NULL;
  no_diff_means_no_output = true;
  BE (pe) = X_DEFAULT;

  /* Room for runv extra + args [+ --binary] [+ 2 labels]
     + 1 file + 1 trailing null.  */
  diffv = pointer_array (PLEXUS, (1 + argc
                                  + !!OPEN_O_BINARY
                                  + 2 * DIFF_L + 2));
  diffp = diffv + 1;
  *diffp++ = prog_diff;

  argc = getRCSINIT (argc, argv, &newargv);
  argv = newargv;
  while (a = *++argv, 0 < --argc && *a++ == '-')
    {
      dcp = a;
      while ((c = *a++))
        switch (c)
          {
          case 'r':
            switch (++revnums)
              {
              case 1:
                rev1 = a;
                break;
              case 2:
                rev2 = a;
                break;
              default:
                PERR ("too many %ss", ks_revno);
              }
            goto option_handled;
          case '-':
          case 'D':
            if ('D' == c
                /* Previously, any long opt would disable the
                   optimization.  Now, we are more refined.  */
                || longopt_maybe_p (*argv, &minus_D)
                || longopt_maybe_p (*argv, &minus_y))
              no_diff_means_no_output = false;
            /* fall into */
          case 'C':
          case 'F':
          case 'I':
          case 'L':
          case 'U':
          case 'W':
#if DIFF_L
            if (c == 'L' && ++file_labels == 2)
              PFATAL ("too many -L options");
#endif
            *dcp++ = c;
            if (*a)
              do
                *dcp++ = *a++;
              while (*a);
            else
              {
                if (!--argc)
                  PFATAL ("-%c needs following argument", c);
                *diffp++ = *argv++;
              }
            break;
          case 'y':
            no_diff_means_no_output = false;
            /* fall into */
          case 'B':
          case 'H':
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case 'a':
          case 'b':
          case 'c':
          case 'd':
          case 'e':
          case 'f':
          case 'h':
          case 'i':
          case 'n':
          case 'p':
          case 't':
          case 'u':
          case 'w':
            *dcp++ = c;
            break;
          case 'q':
            BE (quiet) = true;
            break;
          case 'x':
            suffixarg = *argv;
            BE (pe) = *argv + 2;
            goto option_handled;
          case 'z':
            zonearg = *argv;
            zone_set (*argv + 2);
            goto option_handled;
          case 'T':
            /* Ignore ‘-T’, so that env var ‘RCSINIT’ can contain ‘-T’.  */
            if (*a)
              goto unknown;
            break;
          case 'V':
            versionarg = *argv;
            setRCSversion (versionarg);
            goto option_handled;
          case 'k':
            expandarg = *argv;
            if (0 <= str2expmode (expandarg + 2))
              goto option_handled;
            /* fall into */
          default:
          unknown:
            bad_option (*argv);
          };
    option_handled:
      if (dcp != *argv + 1)
        {
          *dcp = '\0';
          *diffp++ = *argv;
        }
    }
  /* (End of option processing.)  */

  if (! BE (quiet))
    {
      size_t len;

      for (pp = diffv + 2; pp < diffp;)
        accf (PLEXUS, " %s", *pp++);
      diffvstr = finish_string (PLEXUS, &len);
    }

#if DIFF_L
  diff_label1 = diff_label2 = NULL;
  if (file_labels < 2)
    {
      if (!file_labels)
        diff_label1 = diffp++;
      diff_label2 = diffp++;
    }
#endif
  diffpend = diffp;

  cov[1] = PEER_SUPER ();
  cov[2] = "co";
  cov[3] = "-q";
#if !DIFF_L
  cov[COMMAND_LINE_VARYING - 1] = "-M";
#endif

  /* Now handle all filenames.  */
  if (FLOW (erroneousp))
    cleanup (&exitstatus, &work);
  else if (argc < 1)
    PFATAL ("no input file");
  else
    for (; 0 < argc; cleanup (&exitstatus, &work), ++argv, --argc)
      {
        struct cbuf numericrev;
        struct delta *tip;
        char const *mani_filename, *defbr;
        int kws;

        ffree ();

        if (pairnames (argc, argv, rcsreadopen, true, false) <= 0)
          continue;
        tip = REPO (tip);
        mani_filename = MANI (filename);
        kws = BE (kws);
        defbr = GROK (branch);
        diagnose ("%sRCS file: %s", equal_line + 10, REPO (filename));
        if (!rev2)
          {
            /* Make sure work file is readable, and get its status.  */
            if (!(work.fro = fro_open (mani_filename, FOPEN_R_WORK, &work.st)))
              {
                syserror_errno (mani_filename);
                continue;
              }
          }

        if (!tip)
          {
            RERR ("no revisions present");
            continue;
          }
        if (revnums == 0 || !*rev1)
          rev1 = defbr ? defbr : tip->num;

        if (!fully_numeric (&numericrev, rev1, work.fro))
          continue;
        if (! (target = delta_from_ref (numericrev.string)))
          continue;
        xrev1 = target->num;
#if DIFF_L
        if (diff_label1)
          *diff_label1 = setup_label (target->num, target->date);
#endif

        lexpandarg = expandarg;
        if (revnums == 2)
          {
            if (!fully_numeric (&numericrev,
                                *rev2 ? rev2 : (defbr
                                                ? defbr
                                                : tip->num),
                                work.fro))
              continue;
            if (! (target = delta_from_ref (numericrev.string)))
              continue;
            xrev2 = target->num;
            if (no_diff_means_no_output && xrev1 == xrev2)
              continue;
          }
        else if (target->lockedby
                 && !lexpandarg
                 && kws == kwsub_kv
                 && WORKMODE (REPO (stat).st_mode, true) == work.st.st_mode)
          lexpandarg = "-kkvl";
        fro_zclose (&work.fro);
#if DIFF_L
        if (diff_label2)
          {
            if (revnums == 2)
              *diff_label2 = setup_label (target->num, target->date);
            else
              {
                time2date (work.st.st_mtime, date2);
                *diff_label2 = setup_label (NULL, date2);
              }
          }
#endif

        commarg = minus_p (xrev1, rev1);

        pp = &cov[COMMAND_LINE_VARYING];
        *pp++ = commarg.string;
        if (lexpandarg)
          *pp++ = lexpandarg;
        if (suffixarg)
          *pp++ = suffixarg;
        if (versionarg)
          *pp++ = versionarg;
        if (zonearg)
          *pp++ = zonearg;
        *pp++ = REPO (filename);
        *pp = '\0';

        diffp = diffpend;
#if OPEN_O_BINARY
        if (kws == kwsub_b)
          *diffp++ = "--binary";
#endif
        diffp[0] = maketemp (0);
        if (runv (-1, diffp[0], cov))
          {
            RERR ("co failed");
            continue;
          }
        if (!rev2)
          {
            diffp[1] = mani_filename;
            if (*mani_filename == '-')
              {
                accf (PLEXUS, ".%c", SLASH);
                diffp[1] = str_save (mani_filename);
              }
          }
        else
          {
            commarg = minus_p (xrev2, rev2);
            cov[COMMAND_LINE_VARYING] = commarg.string;
            diffp[1] = maketemp (1);
            if (runv (-1, diffp[1], cov))
              {
                RERR ("co failed");
                continue;
              }
          }
        if (!rev2)
          diagnose ("diff%s -r%s %s", diffvstr, xrev1, mani_filename);
        else
          diagnose ("diff%s -r%s -r%s", diffvstr, xrev1, xrev2);

        diffp[2] = 0;
        {
          int s = runv (-1, NULL, diffv);

          if (DIFF_TROUBLE == s)
            MERR ("diff failed");
          if (DIFF_FAILURE == s
              && DIFF_SUCCESS == exitstatus)
            exitstatus = s;
        }
      }

  tempunlink ();
  gnurcs_goodbye ();
  return exitstatus;
}

const uint8_t rcsdiff_aka[14] =
{
  2 /* count */,
  4,'d','i','f','f',
  7,'r','c','s','d','i','f','f'
};

/*:help
[options] file ...
Options:
  -rREV         (zero, one, or two times) Name a revision.
  -kSUBST       Substitute using mode SUBST (see co(1)).
  -q            Quiet mode.
  -V            Like --version.
  -VN           Emulate RCS version N.
  -xSUFF        Specify SUFF as a slash-separated list of suffixes
                used to identify RCS file names.
  -zZONE        Specify date output format in keyword-substitution.

If given two revisions (-rREV1 -rREV2), compare those revisions.
If given only one revision (-rREV), compare the working file with it.
If given no revisions, compare the working file with the latest
revision on the default branch.

Additionally, the following options (and their argument, if any) are
passed to the underlying diff(1) command:
  -0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -B, -C, -D, -F, -H, -I,
  -L, -U, -W, -a, -b, -c, -d, -e, -f, -h, -i, -n, -p, -t, -u, -w, -y,
  [long options (that start with "--")].
(Not all of these options are meaningful.)
*/

/* rcsdiff.c ends here */

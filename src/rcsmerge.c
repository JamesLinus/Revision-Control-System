/* Merge RCS revisions.

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
#include "rcsmerge.help"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-merger.h"
#include "b-peer.h"

#define quietarg  "-q"

int
rcsmerge_main (const char *cmd, int argc, char **argv)
{
  register int i;
  char *a, **newargv;
  struct symdef three_manifestations[3];
  char const *rev[3];                   /*revision numbers */
  char const *edarg, *expandarg, *suffixarg, *versionarg, *zonearg;
  bool tostdout;
  int status, exitstatus;
  struct fro *workptr;
  struct delta *target;
  const struct program program =
    {
      .invoke = argv[0],
      .name = cmd,
      .help = rcsmerge_help,
      .tyag = BOG_DIFF
    };

  CHECK_HV ();
  gnurcs_init (&program);

  edarg = rev[1] = rev[2] = NULL;
  status = 0;                           /* Keep lint happy.  */
  tostdout = false;
  expandarg = suffixarg = versionarg = zonearg = quietarg;      /* no-op */
  BE (pe) = X_DEFAULT;

  argc = getRCSINIT (argc, argv, &newargv);
  argv = newargv;
  while (a = *++argv, 0 < --argc && *a++ == '-')
    {
      switch (*a++)
        {
        case 'p':
          tostdout = true;
          goto revno;

        case 'q':
          BE (quiet) = true;
        revno:
          if (!*a)
            break;
          /* falls into -r */
        case 'r':
          if (!rev[1])
            rev[1] = a;
          else if (!rev[2])
            rev[2] = a;
          else
            PERR ("too many %ss", ks_revno);
          break;

        case 'A':
        case 'E':
        case 'e':
          if (*a)
            goto unknown;
          edarg = *argv;
          break;

        case 'x':
          suffixarg = *argv;
          BE (pe) = a;
          break;
        case 'z':
          zonearg = *argv;
          zone_set (a);
          break;
        case 'T':
          /* Ignore ‘-T’, so that env var ‘RCSINIT’ can contain ‘-T’.  */
          if (*a)
            goto unknown;
          break;
        case 'V':
          versionarg = *argv;
          setRCSversion (versionarg);
          break;

        case 'k':
          expandarg = *argv;
          if (0 <= str2expmode (expandarg + 2))
            break;
          /* fall into */
        default:
        unknown:
          bad_option (*argv);
        };
    }
  /* (End of option processing.)  */

  if (!rev[1])
    PFATAL ("no base %s given", ks_revno);

  /* Now handle all filenames.  */
  if (!FLOW (erroneousp))
    {
      if (argc < 1)
        PFATAL ("no input file");
      if (0 < pairnames (argc, argv, rcsreadopen, true, false))
        {
          struct cbuf numericrev;
          char const *repo_filename = REPO (filename);
          char const *mani_filename = MANI (filename);
          char const *defbr = GROK (branch);
          struct delta *tip = REPO (tip);

          if (argc > 2 || (argc == 2 && argv[1]))
            PWARN ("excess arguments ignored");
          if (BE (kws) == kwsub_b)
            MERR ("merging binary files");
          diagnose ("RCS file: %s", repo_filename);
          if (!(workptr = fro_open (mani_filename, FOPEN_R_WORK, NULL)))
            fatal_sys (mani_filename);

          if (!tip)
            RFATAL ("no revisions present");

          if (!*rev[1])
            rev[1] = defbr ? defbr : tip->num;
          if (fully_numeric (&numericrev, rev[1], workptr)
              && (target = delta_from_ref (numericrev.string)))
            {
              LABEL (1) = target->num;
              if (!rev[2] || !*rev[2])
                rev[2] = defbr ? defbr : tip->num;
              if (fully_numeric (&numericrev, rev[2], workptr)
                  && (target = delta_from_ref (numericrev.string)))
                {
                  LABEL (2) = target->num;

                  if (STR_SAME (LABEL (1), LABEL (2)))
                    {
                      if (tostdout)
                        {
                          fro_spew (workptr, stdout);
                          fclose (stdout);
                        }
                    }
                  else
                    {
                      fro_zclose (&workptr);

                      for (i = 1; i <= 2; i++)
                        {
                          struct cbuf commarg = minus_p (LABEL (i), rev[i]);

                          if (run (-1,
                                   /* Don't collide with merger.c ‘maketemp’.  */
                                   FNAME (i) = maketemp (i + 2),
                                   PEER_SUPER (), "co", quietarg, commarg.string,
                                   expandarg, suffixarg, versionarg, zonearg,
                                   repo_filename, NULL))
                            RFATAL ("co failed");
                        }
                      diagnose
                        ("Merging differences between %s and %s into %s%s",
                         LABEL (1), LABEL (2), mani_filename,
                         tostdout ? "; result to stdout" : "");

                      FNAME (0) = LABEL (0) = mani_filename;
                      status = merge (tostdout, edarg, three_manifestations);
                    }
                }
            }

          fro_zclose (&workptr);
        }
    }
  tempunlink ();
  exitstatus = FLOW (erroneousp) ? DIFF_TROUBLE : status;
  gnurcs_goodbye ();
  return exitstatus;
}

const uint8_t rcsmerge_aka[16] =
{
  2 /* count */,
  5,'m','e','r','g','e',
  8,'r','c','s','m','e','r','g','e'
};

/*:help
[options] file
Options:
  -p[REV]       Write to stdout instead of overwriting the working file.
  -q[REV]       Quiet mode.
  -rREV         (one or two times) specify a revision.
  -kSUBST       Substitute using mode SUBST (see co(1)).
  -V            Like --version.
  -VN           Emulate RCS version N.
  -xSUFF        Specify SUFF as a slash-separated list of suffixes
                used to identify RCS file names.
  -zZONE        Specify date output format in keyword-substitution.

One or two revisions must be specified (using -p, -q, or -r).
If only one is specified, use the latest revision on the default
branch to be the second revision.
*/

/* rcsmerge.c ends here */

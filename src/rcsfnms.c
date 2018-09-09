/* RCS filename handling

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
#include <unistd.h>
#include "same-inode.h"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-feph.h"
#include "b-fro.h"
#include "b-grok.h"

#define rcsdir     "RCS"
#define rcsdirlen  (sizeof rcsdir - 1)

struct compair
{
  char const *suffix, *comlead;
};

/* This table is present only for backwards compatibility.  Normally we
   ignore this table, and use the prefix of the ‘$Log’ line instead.  */
static struct compair const comtable[] = {
  {"a",    "-- "},              /* Ada */
  {"ada",  "-- "},
  {"adb",  "-- "},
  {"ads",  "-- "},
  {"asm",  ";; "},              /* assembler (MS-DOS) */
  {"bat",  ":: "},              /* batch (MS-DOS) */
  {"body", "-- "},              /* Ada */
  {"c",    " * "},              /* C */
  {"c++",  "// "},              /* C++ in all its infinite guises */
  {"cc",   "// "},
  {"cpp",  "// "},
  {"cxx",  "// "},
  {"cl",   ";;; "},             /* Common Lisp */
  {"cmd",  ":: "},              /* command (OS/2) */
  {"cmf",  "c "},               /* CM Fortran */
  {"cs",   " * "},              /* C* */
  {"el",   "; "},               /* Emacs Lisp */
  {"f",    "c "},               /* Fortran */
  {"for",  "c "},
  {"h",    " * "},              /* C-header */
  {"hpp",  "// "},              /* C++ header */
  {"hxx",  "// "},
  {"l",    " * "},              /* lex (NOTE: franzlisp disagrees) */
  {"lisp", ";;; "},             /* Lucid Lisp */
  {"lsp",  ";; "},              /* Microsoft Lisp */
  {"m",    "// "},              /* Objective C */
  {"mac",  ";; "},              /* macro (DEC-10, MS-DOS, PDP-11, VMS, etc) */
  {"me",   ".\\\" "},           /* troff -me */
  {"ml",   "; "},               /* mocklisp */
  {"mm",   ".\\\" "},           /* troff -mm */
  {"ms",   ".\\\" "},           /* troff -ms */
  {"p",    " * "},              /* Pascal */
  {"pas",  " * "},
  {"ps",   "% "},               /* PostScript */
  {"spec", "-- "},              /* Ada */
  {"sty",  "% "},               /* LaTeX style */
  {"tex",  "% "},               /* TeX */
  {"y",    " * "},              /* yacc */
  {NULL,   "# "}                /* default for unknown suffix; must be last */
};

static void
InitAdmin (void)
/* Initialize an admin node.  */
{
  register char const *ext;

  REPO (tip) = NULL;
  BE (strictly_locking) = STRICT_LOCKING;
  REPO (r) = empty_repo (SINGLE);

  /* Guess the comment leader from the suffix.  */
  ext = (ext = strrchr (MANI (filename), '.'))
    ? 1 + ext
    /* Empty suffix; will get default.  */
    : "";
  for (struct compair const *ent = comtable; ; ent++)
    if (!ent->suffix || !strcasecmp (ent->suffix, ext))
      {
        REPO (log_lead).string = ent->comlead;
        REPO (log_lead).size = strlen (ent->comlead);
        break;
      }
  BE (kws) = kwsub_kv;
}

char const *
basefilename (char const *p)
/* Return the address of the base filename of the filename ‘p’.  */
{
  register char const *b = p, *q = p;

  for (;;)
    switch (*q++)
      {
      case SLASHes:
        b = q;
        break;
      case 0:
        return b;
      }
}

static size_t
suffixlen (char const *x)
/* Return the length of ‘x’, an RCS filename suffix.  */
{
  register char const *p;

  p = x;
  for (;;)
    switch (*p)
      {
      case 0:
      case SLASHes:
        return p - x;

      default:
        ++p;
        continue;
      }
}

char const *
rcssuffix (char const *name)
/* Return the suffix of ‘name’ if it is an RCS filename, NULL otherwise.  */
{
  char const *x, *p, *nz;
  size_t nl, xl;

  nl = strlen (name);
  nz = name + nl;
  x = BE (pe);
  do
    {
      if ((xl = suffixlen (x)))
        {
          if (xl <= nl && MEM_SAME (xl, (p = nz - xl), x))
            return p;
        }
      else
        for (p = name; p < nz - rcsdirlen; p++)
          if (isSLASH (p[rcsdirlen])
              && (p == name || isSLASH (p[-1]))
              && MEM_SAME (rcsdirlen, p, rcsdir))
            return nz;
      x += xl;
    }
  while (*x++);
  return NULL;
}

struct fro *
rcsreadopen (struct maybe *m)
/* Open ‘m->tentative’ for reading and return its ‘fro*’ descriptor.
   If successful, set ‘*(m->status)’ to its status.
   Pass this routine to ‘pairnames’ for read-only access to the file.  */
{
  return fro_open (m->tentative.string, FOPEN_RB, m->status);
}

static bool
finopen (struct maybe *m)
/* Use ‘m->open’ to open an RCS file; ‘m->mustread’ is set if the file must be
   read.  Set ‘FLOW (from)’ to the result and return true if successful.
   ‘m->tentative’ holds the file's name.  Set ‘m->bestfit’ to the best RCS name
   found so far, and ‘m->eno’ to its errno.  Return true if successful or if
   an unusual failure.  */
{
  bool interesting, preferold;

  /* We prefer an old name to that of a nonexisting new RCS file,
     unless we tried locking the old name and failed.  */
  preferold = m->bestfit.string[0] && (m->mustread || 0 <= REPO (fd_lock));

  FLOW (from) = (m->open) (m);
  interesting = FLOW (from) || errno != ENOENT;
  if (interesting || !preferold)
    {
      /* Use the new name.  */
      m->eno = errno;
      m->bestfit = m->tentative;
    }
  return interesting;
}

static bool
fin2open (char const *d, size_t dlen,
          char const *base, size_t baselen,
          char const *x, size_t xlen,
          struct maybe *m)
/* ‘d’ is a directory name with length ‘dlen’ (including trailing slash).
   ‘base’ is a filename with length ‘baselen’.
   ‘x’ is an RCS filename suffix with length ‘xlen’.
   Use ‘m->open’ to open an RCS file; ‘m->mustread’ is set if the file
   must be read.  Return true if successful.  Try "dRCS/basex" first; if
   that fails and x is nonempty, try "dbasex".  Put these potential
   names in ‘m->tentative’ for ‘finopen’ to wrangle.  */
{
#define ACC(start)  accumulate_range (m->space, start, start + start ## len)
#define OK()  m->tentative.string = finish_string (m->space, &m->tentative.size)

  /* Try "dRCS/basex".  */
  ACC (d);
  ACC (rcsdir);
  accumulate_byte (m->space, SLASH);
  ACC (base);
  ACC (x);
  OK ();
  if (xlen)
    {
      if (finopen (m))
        return true;

      /* Try "dbasex".  Start from scratch, because
         ‘finopen’ may have changed ‘m->filename’.  */
      ACC (d);
      ACC (base);
      ACC (x);
      OK ();
    }
  return finopen (m);

#undef OK
#undef ACC
}

int
pairnames (int argc, char **argv, open_rcsfile_fn *rcsopen,
           bool mustread, bool quiet)
/* Pair the filenames pointed to by ‘argv’; ‘argc’ indicates how many there
   are.  Place a pointer to the RCS filename into ‘REPO (filename)’, and a
   pointer to the filename of the working file into ‘MANI (filename)’.  If
   both are given, and ‘MANI (standard_output)’ is set, display a warning.

   If the RCS file exists, place its status into ‘REPO (stat)’, open it for
   reading (using ‘rcsopen’), place the file pointer into ‘FLOW (from)’, read
   in the admin-node, and return 1.

   If the RCS file does not exist and ‘mustread’, display an error unless
   ‘quiet’ and return 0.  Otherwise, initialize the admin node and return -1.

   Return 0 on all errors, e.g. files that are not regular files.  */
{
  register char *p, *arg, *RCS1;
  char const *base, *RCSbase, *x;
  char *mani_filename;
  bool paired;
  size_t arglen, dlen, baselen, xlen;
  struct fro *from;
  struct maybe maybe =
    {
      /* ‘.filename’ initialized by ‘fin2open’.  */
      .open = rcsopen,
      .mustread = mustread,
      .status = &REPO (stat)
    };

  REPO (fd_lock) = -1;

  if (!(arg = *argv))
    return 0;                   /* already paired filename */
  if (*arg == '-')
    {
      PERR ("%s option is ignored after filenames", arg);
      return 0;
    }

  base = basefilename (arg);
  paired = false;

  /* First check suffix to see whether it is an RCS file or not.  */
  if ((x = rcssuffix (arg)))
    {
      /* RCS filename given.  */
      RCS1 = arg;
      RCSbase = base;
      baselen = x - base;
      if (1 < argc
          && !rcssuffix (mani_filename = p = argv[1])
          && baselen <= (arglen = (size_t) strlen (p))
          && ((p += arglen - baselen) == mani_filename || isSLASH (p[-1]))
          && MEM_SAME (baselen, base, p))
        {
          argv[1] = NULL;
          paired = true;
        }
      else
        {
          mani_filename = intern (SINGLE, base, baselen + 1);
          mani_filename[baselen] = '\0';
        }
    }
  else
    {
      /* Working file given; now try to find RCS file.  */
      mani_filename = arg;
      baselen = strlen (base);
      /* Derive RCS filename.  */
      if (1 < argc
          && (x = rcssuffix (RCS1 = argv[1]))
          && RCS1 + baselen <= x
          && ((RCSbase = x - baselen) == RCS1 || isSLASH (RCSbase[-1]))
          && MEM_SAME (baselen, base, RCSbase))
        {
          argv[1] = NULL;
          paired = true;
        }
      else
        RCSbase = RCS1 = NULL;
    }
  MANI (filename) = mani_filename;
  /* Now we have a (tentative) RCS filename in ‘RCS1’ and ‘MANI (filename)’.
     Next, try to find the right RCS file.  */
  maybe.space = make_space (__func__);
  if (RCSbase != RCS1)
    {
      /* A filename is given; single RCS file to look for.  */
      maybe.bestfit.string = RCS1;
      maybe.bestfit.size = strlen (RCS1);
      maybe.tentative = maybe.bestfit;
      FLOW (from) = (*rcsopen) (&maybe);
      maybe.eno = errno;
    }
  else
    {
      maybe.bestfit.string = "";
      maybe.bestfit.size = 0;
      if (RCS1)
        /* RCS filename was given without a directory component.  */
        fin2open (arg, (size_t) 0, RCSbase, baselen,
                  x, strlen (x), &maybe);
      else
        {
          /* No RCS filename was given.
             Try each suffix in turn.  */
          dlen = base - arg;
          x = BE (pe);
          while (!fin2open (arg, dlen, base, baselen,
                            x, xlen = suffixlen (x), &maybe))
            {
              x += xlen;
              if (!*x++)
                break;
            }
        }
    }
  REPO (filename) = p = intern (SINGLE, maybe.bestfit.string,
                                maybe.bestfit.size);
  FLOW (erroneousp) = false;
  BE (Oerrloop) = false;
  if ((from = FLOW (from)))
    {
      if (!S_ISREG (maybe.status->st_mode))
        {
          PERR ("%s isn't a regular file -- ignored", p);
          return 0;
        }
      REPO (r) = grok_all (SINGLE, from);
      FLOW (to) = NULL;
    }
  else
    {
      if (maybe.eno != ENOENT || mustread || PROB (REPO (fd_lock)))
        {
          if (maybe.eno == EEXIST)
            PERR ("RCS file %s is in use", p);
          else if (!quiet || maybe.eno != ENOENT)
            syserror (maybe.eno, p);
          return 0;
        }
      InitAdmin ();
    };

  if (paired && MANI (standard_output))
    MWARN ("Working file ignored due to -p option");

  PREV (valid) = false;
  close_space (maybe.space);
  return from ? 1 : -1;
}

#ifndef DOUBLE_SLASH_IS_DISTINCT_ROOT
#define DOUBLE_SLASH_IS_DISTINCT_ROOT 0
#endif

static size_t
dir_useful_len (char const *d)
/* ‘d’ names a directory; return the number of bytes of its useful part.  To
   create a file in ‘d’, append a ‘SLASH’ and a file name to the useful part.
   Ignore trailing slashes if possible; not only are they ugly, but some
   non-POSIX systems misbehave unless the slashes are omitted.  */
{
  size_t dlen = strlen (d);

  if (DOUBLE_SLASH_IS_DISTINCT_ROOT && dlen == 2
      && isSLASH (d[0])
      && isSLASH (d[1]))
    --dlen;
  else
    while (dlen && isSLASH (d[dlen - 1]))
      --dlen;
  return dlen;
}

char const *
getfullRCSname (void)
/* Return a pointer to the full filename of the RCS file.
   Remove leading ‘./’.  */
{
  char const *r = REPO (filename);

  if (ABSFNAME (r))
    return r;
  else
    {
      char *cwd;
      char *rv;
      size_t len;

      if (!(cwd = BE (cwd)))
        {
          /* Get working directory for the first time.  */
          char *PWD = cgetenv ("PWD");
          struct stat PWDstat, dotstat;

          if (!((cwd = PWD)
                && ABSFNAME (PWD)
                && !PROB (stat (PWD, &PWDstat))
                && !PROB (stat (".", &dotstat))
                && SAME_INODE (PWDstat, dotstat)))
            {
              size_t sz = 64;

              while (!(cwd = alloc (PLEXUS, __func__, sz),
                       getcwd (cwd, sz)))
                {
                  brush_off (PLEXUS, cwd);
                  if (errno == ERANGE)
                    sz <<= 1;
                  else if ((cwd = PWD))
                    break;
                  else
                    fatal_sys ("getcwd");
                }
            }
          cwd[dir_useful_len (cwd)] = '\0';
          BE (cwd) = cwd;
        }
      /* Remove leading ‘./’s from ‘REPO (filename)’.
         Do not try to handle ‘../’, since removing it may result
         in the wrong answer in the presence of symbolic links.  */
      for (; r[0] == '.' && isSLASH (r[1]); r += 2)
        /* ‘.////’ is equivalent to ‘./’.  */
        while (isSLASH (r[2]))
          r++;
      /* Build full filename.  */
      accf (SINGLE, "%s%c%s", cwd, SLASH, r);
      rv = finish_string (SINGLE, &len);
      return rv;
    }
}

bool
isSLASH (int c)
{
#if !WOE
  return (SLASH == c);
#else
  switch (c)
    {
    case SLASHes:
      return true;
    default:
      return false;
    }
#endif
}

/* rcsfnms.c ends here */

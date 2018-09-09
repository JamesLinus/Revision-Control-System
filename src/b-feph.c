/* b-feph.c --- (possibly) temporary files

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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unistd-safer.h"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-excwho.h"
#include "b-feph.h"

#define SFF_COUNT  (SFFI_NEWDIR + 2)

/* Temp names to be unlinked when done, if they are not 0.
   Must be at least ‘SFF_COUNT’.  */
#define TEMPNAMES  5

struct ephemstuff
{
  char const *standard;
  struct sff *tpnames;
};

#define EPH(x)  (BE (ephemstuff)-> x)

void
init_ephemstuff (void)
{
  BE (sff) = ZLLOC (SFF_COUNT, struct sff);
  BE (ephemstuff) = ZLLOC (1, struct ephemstuff);
  EPH (tpnames) = ZLLOC (TEMPNAMES, struct sff);
}

static void
jam_sff (struct sff *sff, const char *prefix)
/* Set contents of ‘sff->filename’ to the name of a temporary file made
   from a template with that starts with ‘prefix’.  If ‘prefix’ is
   NULL, use the system "temporary directory".  (Specify the empty
   string for cwd.)  If no name is possible, signal a fatal
   error.  Also, set ‘sff->disposition’ to ‘real’.  */
{
  char *fn;
  size_t len;
  int fd;

  if (!prefix)
    {
      if (! EPH (standard))
        {
          char const *dir = NULL;
          char slash[2] = { SLASH, '\0' };

#define TRY(envvarname)                         \
          if (! dir)                            \
            dir = getenv (#envvarname)
          TRY (TMPDIR);                 /* Unix tradition */
          TRY (TMP);                    /* DOS tradition */
          TRY (TEMP);                   /* another DOS tradition */
#undef TRY
          if (! dir)
            dir = P_tmpdir;

          accf (PLEXUS, "%s%s%s", dir,
                SLASH != dir[strlen (dir) - 1] ? slash : "",
                PROGRAM (name));
          EPH (standard) = finish_string (PLEXUS, &len);
        }
      prefix = EPH (standard);
    }
  accf (PLEXUS, "%sXXXXXX", prefix);
  fn = finish_string (PLEXUS, &len);
  /* Support the 8.3 MS-DOG restriction, blech.  Truncate the non-directory
     filename component to two bytes so that the maximum non-extension name
     is 2 + 6 (Xs) = 8.  The extension is left empty.  What a waste.  */
  if ('/' != SLASH)
    {
      char *end = fn + len - 6;
      char *lastsep = strrchr (fn, SLASH);
      char *ndfc = lastsep ? 1 + lastsep : fn;
      char *dot;

      if (ndfc + 2 < end)
        {
          memset (ndfc + 2, 'X', 6);
          *dot = '\0';
        }
      /* If any of the (up to 2) remaining bytes are '.', replace it
         with the lowest (decimal) digit of the pid.  Double blech.  */
      if ((dot = strchr (ndfc, '.')))
        *dot = '0' + getpid () % 10;
    }

  if (PROB (fd = fd_safer (mkstemp (fn))))
    PFATAL ("could not make temporary file name (template \"%s\")", fn);

  close (fd);
  sff->filename = fn;
  sff->disposition = real;
}

#define JAM_SFF(sff,prefix)  jam_sff (&sff, prefix)

char const *
maketemp (int n)
/* Create a unique filename and store it into the ‘n’th slot
   in ‘EPH (tpnames)’ (so that ‘tempunlink’ can unlink the file later).
   Return a pointer to the filename created.  */
{
  if (!EPH (tpnames)[n].filename)
    JAM_SFF (EPH (tpnames)[n], NULL);

  return EPH (tpnames)[n].filename;
}

char const *
makedirtemp (bool isworkfile)
/* Create a unique filename and store it into ‘BE (sff)’.  Because of
   storage in ‘BE (sff)’, ‘dirtempunlink’ can unlink the file later.
   Return a pointer to the filename created.
   If ‘isworkfile’, put it into the working file's directory;
   otherwise, put the unique file in RCSfile's directory.  */
{
  struct sff *sff = BE (sff);
  int slot = SFFI_NEWDIR + isworkfile;

  JAM_SFF (sff[slot], isworkfile
           ? MANI (filename)
           : REPO (filename));
  return sff[slot].filename;
}

void
keepdirtemp (char const *name)
/* Do not unlink ‘name’, either because it's not there any more,
   or because it has already been unlinked.  */
{
  struct sff *sff = BE (sff);

  for (int i = 0; i < SFF_COUNT; i++)
    if (name == sff[i].filename)
      {
        sff[i].disposition = notmade;
        return;
      }
  PFATAL ("keepdirtemp");
}

static void
reap (size_t count, struct sff all[count],
      int (*cut) (char const *filename))
{
  enum maker m;

  if (!all)
    return;

  for (size_t i = 0; i < count; i++)
    if (notmade != (m = all[i].disposition))
      {
        if (effective == m)
          seteid ();
        cut (all[i].filename);
        all[i].filename = NULL;
        if (effective == m)
          setrid ();
        all[i].disposition = notmade;
      }
}

void
tempunlink (void)
/* Clean up ‘maketemp’ files.  May be invoked by signal handler.  */
{
  reap (TEMPNAMES, EPH (tpnames), unlink);
}

void
dirtempunlink (void)
/* Clean up ‘makedirtemp’ files.
   May be invoked by signal handler.  */
{
  reap (SFF_COUNT, BE (sff), un_link);
}

/* b-feph.c ends here */

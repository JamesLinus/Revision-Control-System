/* b-fro.h --- read-only file

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

struct range
{
  off_t beg;
  off_t end;
};

enum readmethod
  {
    RM_MMAP,
    RM_MEM,
    RM_STDIO
  };

struct fro
{
  int fd;
  off_t end;
  enum readmethod rm;
  char *ptr, *lim, *base;
  void (*deallocate) (struct fro *f);
  FILE *stream;
  off_t verbatim;
};

struct atat
{
  size_t count;
  size_t lno;
  size_t line_count;
  struct fro *from;
#if WITH_NEEDEXP
  size_t needexp_count;
  bool (*ineedexp) (struct atat *atat, size_t i);
  union needexp
  {
    uint64_t  direct;
    uint64_t *bitset;
  } needexp;
#endif  /* WITH_NEEDEXP */
  /* NB: All of the preceding members should have an aggregate size
     that is a multiple of 8, so that ‘beg’ is properly aligned.
     This also requires allocation to be aligned.  */
  off_t beg;
  off_t holes[];
};

extern struct fro *fro_open (char const *filename, char const *type,
                             struct stat *status);
extern void fro_zclose (struct fro **p);
extern void fro_close (struct fro *f);
extern off_t fro_tello (struct fro *f);
extern void fro_move (struct fro *f, off_t change);
extern bool fro_try_getbyte (int *c, struct fro *f);
extern void fro_must_getbyte (int *c, struct fro *f);
extern void fro_trundling (bool sequentialp, struct fro *f);
extern void fro_spew_partial (FILE *to, struct fro *f, struct range *r);
extern void fro_spew (struct fro *f, FILE *to);
extern struct cbuf string_from_atat (struct divvy *space, struct atat const *atat);
extern void atat_put (FILE *to, struct atat const *atat);
extern void atat_display (FILE *to, struct atat const *atat,
                          bool ensure_newline_p);

/* Idioms.  */

#define fro_bob(f)  fro_move (f, 0)

#define STDIO_P(f)  (RM_STDIO == (f)->rm)

/* Get a char into ‘c’ from ‘f’, executing statement ‘s’ at EOF.  */
#define GETCHAR_OR(c,f,s)  do                   \
    if (fro_try_getbyte (&(c), (f)))            \
      { s; }                                    \
  while (0)

/* Like ‘GETCHAR_OR’, except EOF is an error.  */
#define GETCHAR(c,f)  fro_must_getbyte (&(c), (f))

/* The (+2) is for "@\n" (or "@;" for ‘comment’ and ‘expand’).  */
#define ATAT_END(atat)       ((atat)->holes[(atat)->count - 1])
#define ATAT_TEXT_END(atat)  (ATAT_END (atat) + 2)

/* Arrange for ‘fro_spew (f, ...)’ to (later) start at ‘pos’.  */
#define VERBATIM(f,pos)     (f)->verbatim = (pos)
#define IGNORE_REST(f)      VERBATIM ((f), (f)->end)
#define SAME_AFTER(f,atat)  VERBATIM ((f), ATAT_TEXT_END (atat))

/* b-fro.h ends here */

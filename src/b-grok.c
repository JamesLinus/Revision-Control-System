/* b-grok.c --- comma-v parsing

   Copyright (C) 2010-2012 Thien-Thi Nguyen

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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <obstack.h>
#include "hash-pjw.h"
#include "b-complain.h"
#include "b-divvy.h"
#include "b-esds.h"
#include "b-fro.h"
#include "b-grok.h"

/* Define to 1 to enable the context stack.  */
#define CONTEXTUAL 0

struct lockdef
{
  char const *login;
  char const *revno;
};

struct notyet
{
  char const *revno;
  char const *next;
  struct link *branches;                /* list of ‘char const *’ */
  struct delta *d;
};

struct grok
{
  int c;
  struct fro *from;
  struct divvy *to;                     /* for caller */
  struct divvy *systolic;               /* internal */
  struct divvy *tranquil;               /* internal */
#if CONTEXTUAL
  struct link *context;
  size_t depth;
#endif  /* CONTEXTUAL */
  struct cbuf xrep;
  size_t lno;
  size_t head_lno;
  struct cbuf bor_no;                   /* branch or revision */
};

#define STRUCTALLOC(to,type)  alloc (to, #type, sizeof (type))

#if CONTEXTUAL
static void
push_context (struct grok *g, char const *context)
{
  g->context = prepend (context, g->context, g->systolic);
  g->depth += 2;
}

static void
pop_context (struct grok *g)
{
  struct link *bye = g->context;

  g->depth -= 2;
  g->context = g->context->next;
  brush_off (g->systolic, bye);
}

#define CBEG(context)  push_context (g, context)
#define CEND()         pop_context (g)

#else  /* !CONTEXTUAL */

#define CBEG(context)
#define CEND()

#endif

static void
ignoble (struct grok *g, char const *fmt, ...)
{
  va_list args;
  struct cbuf msg;
  struct divvy *scratch = g->systolic;
  struct obstack *o = scratch->space;

  /* First, discard work-in-progress cruft.  */
  obstack_free (o, obstack_finish (o));

  va_start (args, fmt);
  obstack_vprintf (o, fmt, args);
  va_end (args);

#if CONTEXTUAL
  while (g->context)
    {
      accf (scratch, "\n from \"%s\"", g->context->entry);
      g->context = g->context->next;
    }
#endif  /* CONTEXTUAL */
  msg.string = finish_string (scratch, &msg.size);
  complain ("\n");
  fatal_syntax (g->lno, "%s", msg.string);
}

#define BUMMER(...)  ignoble (g, __VA_ARGS__)

static void
eof_too_soon (struct grok *g)
{
  BUMMER ("unexpected end of file");
}

#define MORE(g)  GETCHAR_OR (g->c, g->from, eof_too_soon (g))
#define XREP(g)  ((g)->xrep)

static void
skip_whitespace (struct grok *g)
{
  for (;;)
    {
      if ('\n' == g->c)
        g->lno++;
      if (!isspace (g->c))
        return;
      MORE (g);
    }
}

static void
must_read_keyword (struct grok *g, struct tinysym const *kw)
{
  CBEG (TINYS (kw));
  skip_whitespace (g);
  for (size_t i = 0; i < kw->len; i++)
    {
      if (TINYS (kw)[i] != g->c)
        BUMMER ("missing `%s' keyword", TINYS (kw));
      MORE (g);
    }
  XREP (g).string = TINYS (kw);
  XREP (g).size = kw->len;
  CEND ();
}

#define SYNCH(g,kw)  must_read_keyword (g, &TINY (kw))

static bool
probe_keyword (struct grok *g, struct tinysym const *kw)
/* Return true if keyword ‘kw’ was found, else false.  */
{
  off_t was;
  bool rv = true;

  CBEG (TINYS (kw));
  skip_whitespace (g);
  was = fro_tello (g->from);
  for (size_t i = 0; i < kw->len; i++)
    {
      if (! (rv = (TINYS (kw)[i] == g->c)))
        break;
      MORE (g);
    }
  if (rv)
    {
      XREP (g).string = TINYS (kw);
      XREP (g).size = kw->len;
    }
  else
    {
      fro_move (g->from, was - 1);
      MORE (g);
    }
  CEND ();
  return rv;
}

static void
accb (struct grok *g)
{
  accumulate_byte (g->to, g->c);
  MORE (g);
}

static bool
maybe_read_num (struct grok *g, bool must_be_delta_p)
/* Return true and save to ‘to’ if a number was read,
   else false.  */
{
  char *p;
  size_t dots = 0;

  CBEG ("num");
  skip_whitespace (g);
  while ('.' == g->c || isdigit (g->c))
    {
      if (must_be_delta_p)
        dots += ('.' == g->c);
      accb (g);
    }
  p = finish_string (g->to, &XREP (g).size);
  CEND ();
  if (XREP (g).size)
    {
      bool trailing_garbage = (';' != g->c && !isspace (g->c));

      if (trailing_garbage)
        {
          accs (g->to, p);
          while (';' != g->c && !isspace (g->c))
            accb (g);
          p = finish_string (g->to, &XREP (g).size);
        }
      if (trailing_garbage
          || (must_be_delta_p
              && EVENP (dots)))
        BUMMER ("invalid %s: %s", ks_revno, p);
      XREP (g).string = p;
      return true;
    }
  brush_off (g->to, p);
  XREP (g).string = "";
  return false;
}

static void
must_read_num (struct grok *g, char const *role)
{
  if (! maybe_read_num (g, ks_revno == role))
    BUMMER ("missing %s", role);
}

#define MUST_REVNO(g)   must_read_num (g, ks_revno)
#define MAYBE_REVNO(g)  maybe_read_num (g, true)

static bool
maybe_read_snippet (struct grok *g)
{
  char *p;

  CBEG ("snippet");
  skip_whitespace (g);
  while (';' != g->c
         && ':' != g->c
         && !isspace (g->c)
         && UNKN != ctab[g->c])
    accb (g);
  p = finish_string (g->to, &XREP (g).size);
  CEND ();
  if (XREP (g).size)
    {
      XREP (g).string = p;
      return true;
    }
  brush_off (g->to, p);
  XREP (g).string = "";
  return false;
}

static void
must_read_snippet (struct grok *g, char const *role)
{
  if (! maybe_read_snippet (g))
    BUMMER ("missing %s", role);
}

#define MUST_SNIPPET(g,kw)  must_read_snippet (g, TINYKS (kw))

#define MASK_OFFMSB  (1ULL << (sizeof (off_t) * 8 - 1))

static void
start_atat (struct divvy *to, bool blankp)
{
  struct obstack *o = to->space;

  /* We used to check and ensure ‘off_t’ (8-byte) alignment here,
     but that is now done in ‘make_space’ directly.  */

  if (blankp)
    obstack_blank (o, sizeof (struct atat));
}

static struct atat *
finish_atat (struct divvy *to)
{
  struct atat *rv;
  struct obstack *o = to->space;
  size_t hsize = obstack_object_size (o) - sizeof (struct atat);

  rv = obstack_finish (o);
  rv->count = hsize / sizeof (off_t);
  return rv;
}

#define BITPOSMOD64(i)  (1ULL << (i % 64))

#if WITH_NEEDEXP

static bool
atat_ineedexp_many (struct atat *atat, size_t i)
{
  return BITPOSMOD64 (i) & atat->needexp.bitset[i / 64];
}

static bool
atat_ineedexp_few (struct atat *atat, size_t i)
{
  return BITPOSMOD64 (i) & atat->needexp.direct;
}

#endif  /* WITH_NEEDEXP */

#define SETBIT(i,word)  word |= BITPOSMOD64 (i)

#define MANYP(atat,x)  ((8 * sizeof (atat->needexp.direct)) <= (x))

static bool
maybe_read_atat (struct grok *g, struct atat **res)
{
  struct atat *atat;
  off_t beg;
  size_t lno_start;
  bool newlinep = false;

#define POS(adjust)  (fro_tello (g->from) + (adjust))

  CBEG ("atat");
  skip_whitespace (g);
  lno_start = g->lno;
  beg = POS (-1);
  start_atat (g->systolic, true);
  while (SDELIM == g->c)
    {
      off_t hole;
      bool needexp = false;

      MORE (g);
      while (SDELIM != g->c)
        {
          if (WITH_NEEDEXP && KDELIM == g->c)
            needexp = true;
          else if ((newlinep = ('\n' == g->c)))
            g->lno++;
          MORE (g);
        }
      MORE (g);
      hole = (needexp ? MASK_OFFMSB : 0) | POS (SDELIM == g->c ? -1 : -2);
      obstack_grow (g->systolic->space, &hole, sizeof (hole));
    }
  if ((atat = finish_atat (g->systolic)))
    {
      size_t count = atat->count;
#if WITH_NEEDEXP
      bool many;

      atat->needexp_count = 0;
      if ((many = MANYP (atat, count)))
        atat->needexp.bitset =
          zlloc (g->to, "needexp.bitset",
                 (1 + count / 64) * sizeof (*atat->needexp.bitset));
      else
        atat->needexp.direct = 0;
      atat->ineedexp = many
        ? atat_ineedexp_many
        : atat_ineedexp_few;
      for (size_t i = 0; i < count; i++)
        if (MASK_OFFMSB & atat->holes[i])
          {
            if (many)
              SETBIT (i, atat->needexp.bitset[i / 64]);
            else
              SETBIT (i, atat->needexp.direct);
            atat->needexp_count++;
            atat->holes[i] &= ~MASK_OFFMSB;
          }
#endif  /* WITH_NEEDEXP */

      atat->lno = lno_start;
      atat->line_count = g->lno - atat->lno + !newlinep;
      atat->beg = beg;
      atat->from = g->from;

      /* Allocate a copy.  FIXME: Should not be necessary!
         (Unfortunately, attempts to save to ‘g->to’ directly resulted in a
         segfault traced to ‘gmtime_r’ corrupting the hash table!  Weird!)  */
      start_atat (g->to, false);
      *res = obstack_copy (g->to->space, atat, (sizeof (struct atat)
                                                + count * sizeof (off_t)));
    }

  CEND ();
  return atat;
}

static void
must_read_atat (struct grok *g, struct atat **res, char const *role)
{
  if (! maybe_read_atat (g, res))
    BUMMER ("missing %s", role);
}

#define MUST_ATAT(g,res,kw)  must_read_atat (g, res, TINYKS (kw))

static void
must_colon_revno (struct grok *g, char const *role)
{
  CBEG (role);
  CBEG ("colon");
  /* NB: Don't skip whitespace.  */
  if (':' != g->c)
    BUMMER ("missing ':' in %s", role);
  MORE (g);
  CEND ();
  must_read_num (g, g->bor_no.string);
  CEND ();
}

static void
must_semi (struct grok *g, char const *clause)
{
  size_t was;

  was = g->lno;
  skip_whitespace (g);
  if (';' != g->c)
    BUMMER ("missing semicolon after `%s'", clause);
  MORE (g);
}

#define SEMI(g,kw)  must_semi (g, TINYKS (kw))


struct hash
{
  size_t sz;
  struct wlink **a;
};

static struct hash *
make_hash_table (struct divvy *to, size_t sz)
{
  struct hash *ht = alloc (to, "hash-table", sizeof (struct hash));

  ht->sz = sz;
  ht->a = zlloc (to, "hash-table buckets", sz * sizeof (struct wlink *));
  return ht;
}

static size_t
hash (char const *key, struct hash *ht)
{
  return hash_pjw (key, ht->sz);
}

static void
puthash (struct divvy *to, struct notyet *ny, struct hash *ht)
{
  size_t slot = hash (ny->revno, ht);
  struct wlink box, *tp, *cur;

  box.next = ht->a[slot];
  tp = &box;
  while ((cur = tp->next))
    {
      struct notyet *maybe = cur->entry;

      if (STR_SAME (ny->revno, maybe->revno))
        {
          tp->entry = ny;
          return;
        }
      tp = tp->next;
    }
  tp = wextend (tp, ny, to);
  ht->a[slot] = box.next;
}

static void *
gethash (char const *revno, struct hash *ht)
{
  size_t slot = hash (revno, ht);

  for (struct wlink *ls = ht->a[slot]; ls; ls = ls->next)
    {
      struct notyet *ny = ls->entry;

      if (STR_SAME (revno, ny->revno))
        return ny;
    }
  return NULL;
}


struct fwref
{
  char const *revno;
  size_t lno;                           /* zeroed on match */
};

struct repo *
empty_repo (struct divvy *to)
{
  struct repo *repo = zlloc (to, "repo", sizeof (struct repo));

  repo->strict = STRICT_LOCKING;
  repo->expand = -1;
  repo->neck = -1;
  return repo;
}

/* A nice prime of monolithic proportions.  */
#define NSLOTS  149

static const char ks_ner[] = "non-existent revision";

#define FIND_NY(revno)  gethash (revno, repo->ht)

static struct repo *
full (struct divvy *to, struct fro *f)
{
  off_t neck;
  size_t count;
  struct link box, *tp;
  struct wlink *follow;
  struct wlink *all_br = NULL;
  struct grok *g = FZLLOC (struct grok);
  struct repo *repo = empty_repo (to);

  repo->ht = make_hash_table (to, NSLOTS);

  g->from = f;
  g->to = to;
  g->systolic = make_space ("systolic");
  g->tranquil = make_space ("tranquil");
  g->lno = 1;
  accf (g->tranquil, "branch or %s", ks_revno);
  g->bor_no.string = finish_string (g->tranquil, &g->bor_no.size);
  MORE (g);

#define STASH(cvar)  cvar = XREP (g).string
#define PREP(field)  count = 0, box.next = repo->field, tp = &box
#define HANG(x)      tp = extend (tp, x, to)
#define DONE(field)  repo->field = box.next, repo->field ## _count = count

  CBEG ("admin node");

  SYNCH (g, head);
  if (MAYBE_REVNO (g))
    g->head_lno = g->lno, STASH (repo->head);
  SEMI (g, head);

  if (probe_keyword (g, &TINY (branch)))
    {
      if (maybe_read_num (g, false))
        STASH (repo->branch);
      SEMI (g, branch);
    }

#if COMPAT2
  /* Read and ignore suffix.  Only in release 2 format.  */
  if (probe_keyword (g, &TINY (suffix)))
    {
      struct atat ignore, *really = &ignore;

      if (maybe_read_atat (g, &really))
        brush_off (to, really);
      else
        /* Don't bother brushing off the snippet (thus avoiding a
           compiler warning due to passing ‘brush_off’ a ‘const’ arg).  */
        maybe_read_snippet (g);
      SEMI (g, suffix);
    }
#endif  /* COMPAT2 */

  SYNCH (g, access);
  for (PREP (access); maybe_read_snippet (g); count++)
    HANG (XREP (g).string);
  DONE (access);
  SEMI (g, access);

  SYNCH (g, symbols);
  for (PREP (symbols); maybe_read_snippet (g); count++)
    {
      struct symdef *sym = STRUCTALLOC (to, struct symdef);

      STASH (sym->meaningful);
      must_colon_revno (g, "symbolic name definition");
      STASH (sym->underlying);
      HANG (sym);
    }
  DONE (symbols);
  SEMI (g, symbols);

  SYNCH (g, locks);
  for (PREP (locks); maybe_read_snippet (g); count++)
    {
      struct lockdef *lock = STRUCTALLOC (to, struct lockdef);

      STASH (lock->login);
      must_colon_revno (g, "locker definition");
      STASH (lock->revno);
      HANG (lock);
    }
  DONE (locks);
  SEMI (g, locks);
  /* ci(1) might call ‘grok_resynch’, after having possibly deleted a lock
     (with a side-effecting ‘setcdr’) so we save a copy of the definitions.
     FIXME: Don't ‘setcdr’!  (Alternatively, remove need to resynch.)  */
  repo->lockdefs = alloc (to, "locker definition",
                          count * sizeof (struct lockdef));
  for (tp = repo->locks; count--; tp = tp->next)
    {
      struct lockdef const *orig = tp->entry;

      repo->lockdefs[count] = *orig;
    }

  if ((repo->strict = probe_keyword (g, &TINY (strict))))
    SEMI (g, strict);

  if (probe_keyword (g, &TINY (integrity)))
    {
      maybe_read_atat (g, &repo->integrity);
      SEMI (g, integrity);
    }

  if (probe_keyword (g, &TINY (comment)))
    {
      maybe_read_atat (g, &repo->comment);
      SEMI (g, comment);
    }

  repo->expand = -1;
  if (probe_keyword (g, &TINY (expand)))
    {
      struct atat *expand;

      if (maybe_read_atat (g, &expand))
        {
          struct cbuf cb = string_from_atat (g->systolic, expand);

          if (PROB (repo->expand = recognize_kwsub (&cb)))
            BUMMER ("invalid expand mode: %s", cb.string);
        }
      SEMI (g, expand);
    }

  CBEG ("revisions");
  {
    struct wlink wbox, *wtp;
    struct notyet *prev = NULL;
    struct fwref *fw;

    for (count = 0, wbox.next = repo->deltas, wtp = &wbox;
         MAYBE_REVNO (g);
         count++)
      {
        struct notyet *ny = STRUCTALLOC (to, struct notyet);
        struct delta *d = ny->d = STRUCTALLOC (to, struct delta);
        size_t numlen = XREP (g).size;

        STASH (d->num);
        /* Check that a new branch is properly forward-referenced.  */
        if (prev && !prev->next
            && 2 <= countnumflds (d->num))
          {
            struct wlink *ls;

            for (ls = all_br; ls; ls = ls->next)
              if ((fw = ls->entry)->lno
                  && STR_SAME (d->num, fw->revno))
                {
                  fw->lno = 0;
                  break;
                }
            if (!ls)
              BUMMER ("unexpected new branch %s: %s", ks_revno, d->num);
          }

        d->branches = NULL;             /* see ‘grok_all’ */
        d->ilk = NULL;                  /* see ‘grok_all’ */
        d->lockedby = NULL;             /* see ‘grok_resynch’ */
        d->pretty_log.string = NULL;
        d->pretty_log.size = 0;
        d->selector = true;
        d->log = NULL;

        STASH (ny->revno);
        CBEG (ny->revno);

        SYNCH (g, date);
        must_read_num (g, "date");
        STASH (d->date);
        SEMI (g, date);

        SYNCH (g, author);
        MUST_SNIPPET (g, author);
        STASH (d->author);
        SEMI (g, author);

        SYNCH (g, state);
        MUST_SNIPPET (g, state);
        STASH (d->state);
        SEMI (g, state);

        SYNCH (g, branches);
        box.next = NULL, tp = &box;
        while (MAYBE_REVNO (g))
          {
            /* Branches must begin with the branch point revision.  */
            if (numlen >= XREP (g).size
                || strncmp (d->num, XREP (g).string, numlen)
                || '.' != XREP (g).string[numlen]
                || 2 != countnumflds (XREP (g).string + numlen + 1))
              BUMMER ("invalid branch `%s' at branchpoint `%s'",
                      XREP (g).string, d->num);
            fw = STRUCTALLOC (g->tranquil, struct fwref);
            fw->revno = XREP (g).string;
            fw->lno = g->lno;
            all_br = wprepend (fw, all_br, g->tranquil);
            HANG (XREP (g).string);
          }
        ny->branches = box.next;
        SEMI (g, branches);

        SYNCH (g, next);
        if (MAYBE_REVNO (g))
          STASH (ny->next);
        else
          ny->next = NULL;
        SEMI (g, next);

        if (probe_keyword (g, &TINY (commitid)))
          {
            MUST_SNIPPET (g, commitid);
            STASH (d->commitid);
            checkssym (d->commitid);
            SEMI (g, commitid);
          }
        else
          d->commitid = NULL;

        CEND ();
        wtp = wextend (wtp, ny, to);
        puthash (to, ny, repo->ht);
        prev = ny;
      }
    /* Check that all forward references were matched.  */
    for (; all_br; all_br = all_br->next)
      if ((fw = all_br->entry)->lno)
        {
          /* Jam line number for error message. */
          g->lno = fw->lno;
          BUMMER ("branch refers to %s `%s'", ks_ner, fw->revno);
        }
    repo->deltas = wbox.next;
    repo->deltas_count = count;
  }
  CEND ();

  SYNCH (g, desc);
  repo->neck = fro_tello (g->from);
  MUST_ATAT (g, &repo->desc, desc);

  CEND ();

#undef EXTEND
#undef PREP
#undef STASH

  /* Handle dangling lockdefs, that is, those whose revno references a
     non-existent delta.  Do it here instead of lazily (in ‘grok_resynch’)
     to avoid emitting multiple warnings.  */
  for (struct lockdef const *lock = repo->lockdefs;
       lock < repo->lockdefs + repo->locks_count;
       lock++)
    {
      struct notyet *ny = FIND_NY (lock->revno);

      if (! ny)
        /* Create a dummy, hashed but not added to ‘repo->deltas’.  */
        {
          RWARN ("user `%s' holds a lock for %s `%s'",
                 lock->login, ks_ner, lock->revno);
          ny = zlloc (to, "dummy ny", sizeof (struct notyet));
          ny->d = zlloc (to, "dummy delta", sizeof (struct delta));
          ny->revno = ny->d->num = lock->revno;
          puthash (to, ny, repo->ht);
        }
    }

  CBEG ("edits");
  for (count = 0, follow = repo->deltas;
       (neck = fro_tello (g->from)) && count < repo->deltas_count;
       count++)
    {
      char const *revno;
      struct notyet *ny;
      struct delta *d;

      MUST_REVNO (g);
      revno = XREP (g).string;
      CBEG (revno);
      if (!(ny = FIND_NY (revno)))
        BUMMER ("found edits for %s `%s'", ks_ner, revno);
      /* TODO: Instead of this (accumulate then reorder),
         delay accumulation then accumulate in order.  */
      follow->entry = ny;
      follow = follow->next;
      d = ny->d;
      if (d->log)
        BUMMER ("duplicate delta log for %s `%s'", ks_revno, d->num);
      d->neck = neck;
      SYNCH (g, log);
      MUST_ATAT (g, &d->log, log);
      SYNCH (g, text);
      MUST_ATAT (g, &d->text, text);
      CEND ();
    }
  CEND ();

  /* Yes, this is what teenagers learn painting
     houses in the summertime.  Recommended!
     | en: dingleberry
     | it: tarzanello
     | es: gamborimbo
     | de: Klabusterbeere
     | no: Danglebær
     | sv: kånkelbär
     | lt: sudgabalis
     Thanks wiktionary.org!  */
  CBEG ("clean tail");
  while (isspace (g->c))
    {
      if ('\n' == g->c)
        g->lno++;
      GETCHAR_OR (g->c, g->from, goto ok);
    }
  BUMMER ("junk at end of file: '%c'", g->c);
 ok:
  CEND ();

  /* Validate ‘GROK (head)’.  */
  if (repo->head && !FIND_NY (repo->head))
    fatal_syntax (g->head_lno, "RCS file head names a %s `%s'",
                  ks_ner, repo->head);

  /* Link deltas (via ‘branches’ and ‘next’).  */
  for (struct wlink *ls = repo->deltas; ls; ls = ls->next)
    {
      struct notyet *ny = ls->entry, *deref;
      struct delta *d = ny->d;

#define FIND_D(revno)   ((deref = FIND_NY (revno))->d)

      if (ny->next)
        d->ilk = FIND_D (ny->next);
      if (ny->branches)
        {
          struct link *bls;
          struct wlink wbox, *wtp;

          for (bls = ny->branches, wbox.next = d->branches, wtp = &wbox;
               bls;
               bls = bls->next)
            wtp = wextend (wtp, FIND_D (bls->entry), to);
          d->branches = wbox.next;
        }
      ls->entry = d;

#undef FIND_D
    }

  /* Don't close ‘g->to’; that is caller's responsability.  */
  close_space (g->systolic);
  close_space (g->tranquil);

  return repo;
}

struct repo *
grok_all (struct divvy *to, struct fro *f)
{
  struct repo *repo = full (to, f);

  grok_resynch (repo);
  return repo;
}

void
grok_resynch (struct repo *repo)
/* (Re-)initialize the appropriate global variables.  */
{
  struct notyet *ny;

  REPO (tip) = repo->head && (ny = FIND_NY (repo->head))
    ? ny->d
    : NULL;

  repo->locks = NULL;
  for (struct lockdef const *orig = repo->lockdefs + repo->locks_count;
       repo->lockdefs < orig-- && (ny = FIND_NY (orig->revno));)
    {
      struct delta *d = ny->d;
      struct rcslock *rl = FALLOC (struct rcslock);

      rl->login = d->lockedby = orig->login;
      rl->delta = d;
      repo->locks = prepend (rl, repo->locks, SINGLE);
    }

  BE (strictly_locking) = repo->strict;

  if (repo->comment)
    REPO (log_lead) = string_from_atat (SINGLE, repo->comment);
  else
    clear_buf (&REPO (log_lead));

  BE (kws) = PROB (repo->expand)
    ? kwsub_kv
    : repo->expand;
}

/* b-grok.c ends here */

/* b-isr.c --- interrupt service routine

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

/* Standard C places too many restrictions on signal handlers.
   We obey as many of them as we can.  POSIX places fewer
   restrictions, and we are POSIX-compatible here.  */

#include "base.h"
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include "b-complain.h"
#include "b-divvy.h"
#include "b-excwho.h"
#include "b-isr.h"

void
maybe_reset_sigchld (void)
{
#if BAD_WAIT_IF_SIGCHLD_IGNORED
#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif
  signal (SIGCHLD, SIG_DFL);
#endif  /* BAD_WAIT_IF_SIGCHLD_IGNORED */
}

/* Avoid calling ‘sprintf’ etc., in case they're not reentrant.  */

static void
werr (char const *s)
{
  ssize_t len;

  if (! (len = strlen (s)))
    return;

  if (len != write (STDERR_FILENO, s, len))
    BOW_OUT ();
}

void
complain_signal (char const *msg, int signo)
{
  werr (msg);
  werr (": ");
  werr (strsignal (signo));
  werr ("\n");
}

struct isr_scratch
{
  sig_atomic_t volatile held, level;
  siginfo_t bufinfo;
  siginfo_t *volatile held_info;
  char const *access_name;
  struct
  {
    bool regular;
    bool memory_map;
  } catching;
  bool *be_quiet;
};

#define ISR(x)  (scratch->x)

void
access_page (struct isr_scratch *scratch,
             char const *filename, char const *p)
{
  char volatile t;

  ISR (access_name) = filename;
  t = *p;
  ISR (access_name) = NULL;
}

static void
ignore (struct isr_scratch *scratch)
{
  ++ISR (level);
}

#if !defined HAVE_PSIGINFO
#define psiginfo(info, msg)  complain_signal (msg, info->si_signo)
#endif

static void
catchsigaction (int signo, siginfo_t *info, RCS_UNUSED void *uc)
{
  struct isr_scratch *scratch = ISR_SCRATCH;
  bool from_mmap = MMAP_SIGNAL && MMAP_SIGNAL == signo;

  if (ISR (level))
    {
      ISR (held) = signo;
      if (info)
        {
          ISR (bufinfo) = *info;
          ISR (held_info) = &ISR (bufinfo);
        }
      return;
    }

  ignore (scratch);
  setrid ();
  if (!*ISR (be_quiet))
    {
      /* If this signal was planned, don't complain about it.  */
      if (!(from_mmap && ISR (access_name)))
        {
          const char *nRCS = "\nRCS";

          if (from_mmap && info && info->si_errno)
            {
              errno = info->si_errno;
              /* Bump start of string to avoid subsequent newline output.  */
              perror (nRCS++);
            }
          if (info)
            psiginfo (info, nRCS);
          else
            complain_signal (nRCS, signo);
        }

      werr ("RCS: ");
      if (from_mmap)
        {
          if (ISR (access_name))
            {
              werr (ISR (access_name));
              werr (": Permission denied.  ");
            }
          else
            werr ("Was a file changed by some other process?  ");
        }
      werr ("Cleaning up.\n");
    }
  BOW_OUT ();
}

#ifndef SA_ONSTACK
#define SA_ONSTACK  0
#endif

static void
setup_catchsig (size_t count, int const set[count])
{
  sigset_t blocked;

#define MUST(x)  if (PROB (x)) goto fail

  sigemptyset (&blocked);
  for (size_t i = 0; i < count; i++)
    MUST (sigaddset (&blocked, set[i]));

  for (size_t i = 0; i < count; i++)
    {
      struct sigaction act;
      int sig = set[i];

      MUST (sigaction (sig, NULL, &act));
      if (SIG_IGN != act.sa_handler)
        {
          act.sa_sigaction = catchsigaction;
          act.sa_flags |= SA_SIGINFO | SA_ONSTACK;
          act.sa_mask = blocked;
          if (PROB (sigaction (sig, &act, NULL)))
            {
            fail:
              fatal_sys ("signal handling");
            }
        }
    }

#undef MUST
}

#if defined HAVE_SIGALTSTACK && defined SIGSTKSZ
#define ISR_STACK_SIZE  (10 * SIGSTKSZ)
#else
#define ISR_STACK_SIZE  0
#endif

struct isr_scratch *
isr_init (bool *be_quiet)
{
  struct isr_scratch *scratch = ZLLOC (1, struct isr_scratch);

#if ISR_STACK_SIZE
  stack_t ss =
    {
      .ss_sp = alloc (PLEXUS, "sigaltstack", ISR_STACK_SIZE),
      .ss_size = ISR_STACK_SIZE,
      .ss_flags = 0
    };

  if (PROB (sigaltstack (&ss, NULL)))
    fatal_sys ("sigaltstack");
#endif

  /* Make peer-subsystem connection.  */
  ISR (be_quiet) = be_quiet;
  return scratch;
}

#define COUNT(array)  (int) (sizeof (array) / sizeof (*array))

void
isr_do (struct isr_scratch *scratch, enum isr_actions action)
{
  switch (action)
    {
    case ISR_CATCHINTS:
      {
        int const regular[] =
          {
            SIGHUP,
            SIGINT,
            SIGQUIT,
            SIGPIPE,
            SIGTERM,
            SIGXCPU,
            SIGXFSZ,
          };

        if (!ISR (catching.regular))
          {
            ISR (catching.regular) = true;
            setup_catchsig (COUNT (regular), regular);
          }
      }
      break;

    case ISR_IGNOREINTS:
      ignore (scratch);
      break;

    case ISR_RESTOREINTS:
      if (!--ISR (level) && ISR (held))
        catchsigaction (ISR (held), ISR (held_info), NULL);
      break;

    case ISR_CATCHMMAPINTS:
      /* If you mmap an NFS file, and someone on another client removes
         the last link to that file, and you later reference an uncached
         part of that file, you'll get a SIGBUS or SIGSEGV (depending on
         the operating system).  Catch the signal and report the problem
         to the user.  Unfortunately, there's no portable way to
         differentiate between this problem and actual bugs in the
         program.  This NFS problem is rare, thank goodness.

         This can also occur if someone truncates the file,
         even without NFS.  */
      {
        int const mmapsigs[] = { MMAP_SIGNAL };

        if (MMAP_SIGNAL && !ISR (catching.memory_map))
          {
            ISR (catching.memory_map) = true;
            setup_catchsig (COUNT (mmapsigs), mmapsigs);
          }
      }
      break;
    }
}

/* b-isr.c ends here */

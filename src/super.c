/* Dispatch an RCS command.

   Copyright (C) 2011, 2012 Thien-Thi Nguyen

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
#include <unistd.h>
#include "super.help"
#include "b-divvy.h"
#include "b-complain.h"
#include "b-peer.h"

/* {Dynamic Root} TODO: Move into library.

   For the present, internal dispatch (e.g., ‘diff’ calls ‘co’) goes
   through execve(2), but the plan is to eventually elide that into
   a function call, at which point dynamic-root support will need to
   move into the library.  */

struct dynamic_root
{
  struct top *top;
  struct divvy *single;
  struct divvy *plexus;
  /* FIXME: What about these?
     - program_invocation_name
     - program_invocation_short_name
     - stderr
     - stdin
     - stdout
     (These are from "nm --defined-only -D grcs".)  */
};

static void
droot_global_to_stack (struct dynamic_root *dr)
{
  dr->top = top;
  dr->single = single;
  dr->plexus = plexus;
}

static void
droot_stack_to_global (struct dynamic_root *dr)
{
  top = dr->top;
  single = dr->single;
  plexus = dr->plexus;
}

typedef int (submain_t) (const char *cmd, int argc, char **argv);

struct aliases
{
  submain_t *func;
  const char *blurb;
  const uint8_t *aka;
};

#define DECLARE_SUBMAIN(prog)   extern submain_t prog ## _main
#define DECLARE_SUBBLURB(prog)  extern const char prog ## _blurb[]
#define DECLARE_SUBAKA(prog)    extern const uint8_t prog ## _aka[]

#define DECLARE_SUB(prog)  \
  DECLARE_SUBMAIN (prog);  \
  DECLARE_SUBBLURB(prog);  \
  DECLARE_SUBAKA (prog)

DECLARE_SUB (ci);
DECLARE_SUB (co);
DECLARE_SUB (rcs);
DECLARE_SUB (rcsclean);
DECLARE_SUB (rcsdiff);
DECLARE_SUB (rcsmerge);
DECLARE_SUB (rlog);

#define SUBENT(prog)  { prog ## _main, prog ## _blurb, prog ## _aka }

struct aliases aliases[] =
  {
    SUBENT (ci),
    SUBENT (co),
    SUBENT (rcs),
    SUBENT (rcsclean),
    SUBENT (rcsdiff),
    SUBENT (rcsmerge),
    SUBENT (rlog)
  };

static const size_t n_aliases = sizeof (aliases) / sizeof (aliases[0]);

static submain_t *
recognize (const char *maybe)
{
  size_t mlen = strlen (maybe);
  size_t i;

  for (i = 0; i < n_aliases; i++)
    {
      struct aliases *a = aliases + i;
      const uint8_t *aka = a->aka;
      size_t count = *aka++;

      while (count--)
        {
          struct tinysym *sym = (struct tinysym *) aka;

          if (mlen == sym->len
              && looking_at (sym, maybe))
            return a->func;
          aka += sym->len + 1;
        }
    }
  return NULL;
}

static void
display_commands (void)
{
  printf ("Commands:\n\n");
  for (size_t i = 0; i < n_aliases; i++)
    {
      struct aliases *a = aliases + i;
      const uint8_t *aka = a->aka;
      size_t count = *aka++;

      for (size_t j = 0; j < count; j++)
        {
          struct tinysym *sym = (struct tinysym *) aka;
          char name[16];

          memcpy (name, sym->bytes, sym->len);
          name[sym->len] = '\0';
          switch (j)
            {
            case 0:
              printf (" %-10s %s\n %10s (aka:", name, a->blurb, "");
              break;
            case 1:
              printf (" %s", name);
              break;
            default:
              printf (", %s", name);
            }
          aka += 1 + sym->len;
        }
      printf (")\n\n");
    }
}

struct top *top;

static char const hint[] = " (try --help)";

static exiting void
huh (const char *what, const char *argv1)
{
  PFATAL ("unrecognized %s: %s%s", what, argv1, hint);
}

#define HUH(what)  huh (what, argv[1])

int
main (int argc, char **argv)
{
  int exitval = EXIT_SUCCESS;
  const struct program program =
    {
      .invoke = argv[0],
      .name = peer_super.meaningful,
      .help = super_help,
      .tyag = TYAG_IMMEDIATE
    };

  CHECK_HV ();
  gnurcs_init (&program);

  if (2 > argc)
    PWARN ("missing command%s", hint);
  else
    {
      const char *cmd;
      submain_t *sub;

      /* Option processing.  */
      if ('-' == argv[1][0])
        {
          if (STR_SAME ("--commands", argv[1]))
            {
              display_commands ();
              goto done;
            }
          HUH ("option");
        }

      /* Try dispatch.  */
      if (! (sub = recognize (cmd = argv[1])))
        HUH ("command");
      else
        {
          struct dynamic_root super;

          /* Construct a simulated invocation.  */
          argv[1] = one_beyond_last_dir_sep (argv[0])
            ? argv[0]
            : str_save (PEER_SUPER ());

          /* Dispatch, surrounded by dynamic-root push/pop.  */
          droot_global_to_stack (&super);
          exitval = sub (cmd, argc - 1, argv + 1);
          droot_stack_to_global (&super);
        }
    }

 done:
  gnurcs_goodbye ();
  return exitval;
}

/*:help
[options] command [command-options...]
Options:
  --help       Display this text and exit.
  --version    Display version information and exit.
  --commands   Display available commands and exit.

To see help for a command, specify the command and --help, e.g.:
  co --help
*/

/* super.c ends here */

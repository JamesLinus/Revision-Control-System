/* gnu-h-v.h --- GNUish --help and --version handling

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

/* Display the version blurb to stdout, starting with:
   | NAME (GNU RCS) PACKAGE_VERSION
   | ...
   and ending with newline.  NAME is the value of ‘prog->name’.  */
extern void
display_version (struct program const *prog);

/* If ARGC is less than 2, do nothing.
   If ARGV[1] is "--version", use ‘display_version’ and exit successfully.
   If ARGV[1] is "--help", display the help blurb, starting with:
   | NAME HELP
   and exit successfully.  NAME is the value of ‘prog->name’,
   while HELP is the value of ‘prog->help’.  */
extern void
check_hv (int argc, char **argv, struct program const *prog);

/* Idiom.  */
#define CHECK_HV()  check_hv (argc, argv, &program)

/* gnu-h-v.h ends here */

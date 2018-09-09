/* b-peer.h --- finding the ‘execv’able name of a peer program

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

extern struct symdef peer_super;

const char *one_beyond_last_dir_sep (const char *name);
char const *find_peer_prog (struct symdef *prog);

/* Idioms.  */
#define PEER_SUPER()  find_peer_prog (&peer_super)

/* b-peer.h ends here */

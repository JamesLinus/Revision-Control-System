/* RCS map of character types

   Copyright (C) 2010-2012 Thien-Thi Nguyen
   Copyright (C) 1990, 1991, 1995 Paul Eggert
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
#include "b-complain.h"

/* Map of character types: ISO 8859/1 (Latin-1).  */
enum tokens const ctab[] =
  {
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    SPACE,  SPACE,  NEWLN,  SPACE,  SPACE,  SPACE,  UNKN,   UNKN,
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    SPACE,  IDCHAR, IDCHAR, IDCHAR, DELIM,  IDCHAR, IDCHAR, IDCHAR,
    IDCHAR, IDCHAR, IDCHAR, IDCHAR, DELIM,  IDCHAR, PERIOD, IDCHAR,
    DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,
    DIGIT,  DIGIT,  COLON,  SEMI,   IDCHAR, IDCHAR, IDCHAR, IDCHAR,
    SBEGIN, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER,
    LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER,
    LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER,
    LETTER, LETTER, LETTER, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR,
    IDCHAR, Letter, Letter, Letter, Letter, Letter, Letter, Letter,
    Letter, Letter, Letter, Letter, Letter, Letter, Letter, Letter,
    Letter, Letter, Letter, Letter, Letter, Letter, Letter, Letter,
    Letter, Letter, Letter, IDCHAR, IDCHAR, IDCHAR, IDCHAR, UNKN,
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,   UNKN,
    IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR,
    IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR,
    IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR,
    IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR, IDCHAR,
    LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER,
    LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER,
    LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, IDCHAR,
    LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, LETTER, Letter,
    Letter, Letter, Letter, Letter, Letter, Letter, Letter, Letter,
    Letter, Letter, Letter, Letter, Letter, Letter, Letter, Letter,
    Letter, Letter, Letter, Letter, Letter, Letter, Letter, IDCHAR,
    Letter, Letter, Letter, Letter, Letter, Letter, Letter, Letter
  };

static char const *
checkidentifier (char const *id, int delimiter, register bool dotok)
/* Check whether the string starting at ‘id’ is an identifier and return
   a pointer to the delimiter after the identifier.  White space,
   ‘delimiter’ and 0 are legal delimiters.  Abort the program if not a
   legal identifier.  Useful for checking commands.  If ‘!delimiter’,
   the only delimiter is 0.  Allow '.' in identifier only if ‘dotok’ is
   set.  */
{
  register char const *temp;
  register char c;
  register char delim = delimiter;
  bool isid = false;

  temp = id;
  for (;; id++)
    {
      switch (ctab[(unsigned char) (c = *id)])
        {
        case DIGIT:
        case IDCHAR:
        case LETTER:
        case Letter:
          isid = true;
          continue;

        case PERIOD:
          if (dotok)
            continue;
          break;

        default:
          break;
        }
      break;
    }
  if (!isid || (c && (!delim || (c != delim
                                 && c != ' '
                                 && c != '\t'
                                 && c != '\n'))))
    {
      while ((c = *id) && c != ' ' && c != '\t' && c != '\n'
             && c != delim)
        id++;
      PFATAL ("invalid %s `%.*s'", dotok ? "identifier" : "symbol",
              (int) (id - temp), temp);
    }
  return id;
}

char const *
checkid (char const *id, int delimiter)
{
  return checkidentifier (id, delimiter, true);
}

char const *
checksym (char const *sym, int delimiter)
{
  return checkidentifier (sym, delimiter, false);
}

void
checksid (char const *id)
/* Check whether the string ‘id’ is an identifier.  */
{
  checkid (id, 0);
}

void
checkssym (char const *sym)
{
  checksym (sym, 0);
}

/* rcsmap.c ends here */

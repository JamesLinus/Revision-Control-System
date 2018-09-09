# Make RCS man pages.

# Copyright (C) 2010-2012 Thien-Thi Nguyen
#
# This file is part of GNU RCS.
#
# GNU RCS is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNU RCS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

dist_man_MANS = ci.1 co.1 ident.1 merge.1 rcs.1 rcsclean.1 \
  rcsdiff.1 rcsfile.5 rcsmerge.1 rlog.1

## Is this correct?
dist_noinst_MANS = rcsfreeze.1

BUILT_SOURCES = REL $(dist_noinst_MANS)

bits = b-base \
  b-emptylogmessage \
  b-environment \
  b-identification \
  b-tichy-cite

EXTRA_DIST = $(dist_man_MANS:=in) $(BUILT_SOURCES) $(bits)
EXTRA_DIST += $(dist_noinst_MANS:=in)

soelim = $(SOELIM) -I. -I'$(srcdir)'
nolf = sed '/^\.lf /d'

# We do the sanity (test -s) check at generation time since
# it is a maintainer concern not a user (make check) concern.

.5in.5:
	$(soelim) $< | $(PIC) $(PICFLAGS) | $(nolf) > $@
	test -s $@

.1in.1:
	$(soelim) $< | $(nolf) > $@
	test -s $@

$(dist_man_MANS) $(dist_noinst_MANS): REL $(bits)

REL: $(top_srcdir)/configure
	@{ echo '.ds Rv $(PACKAGE_VERSION)' ;		\
	   date -u +'.ds Dt %Y-%m-%d' -r $< ; } > $@t ;	\
	if [ -f $@ ] && cmp -s $@t $@ ;			\
	  then rm -f $@t ;				\
	  else mv $@t $@ ; echo Created $@ ;		\
	fi

MAINTAINERCLEANFILES = $(dist_man_MANS) REL

# Makefile.am ends here

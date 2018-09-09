# Make RCS.

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

# This is for the gnulib headers when srcdir != builddir.
# Hmmm, shouldn't gnulib or automake handle this automagically?
AM_CPPFLAGS = -I'$(top_srcdir)/lib'

subs = ci co rcs rcsclean rcsdiff rcsmerge rlog
bin_SCRIPTS = $(subs)

$(subs): sub.TEMPLATE
	sed 's/SUB/$@/;s/VERSION/$(VERSION)/' $< > $@
	chmod +x $@

noinst_LIBRARIES = libparts.a
libparts_a_SOURCES = \
  b-complain.h b-divvy.h b-esds.h b-excwho.h b-fb.h b-feph.h b-fro.h \
  b-grok.h b-isr.h b-kwxout.h b-merger.h b-peer.h \
  base.h gnu-h-v.h maketime.h partime.h \
  b-anchor.c \
  b-complain.c b-divvy.c b-esds.c b-excwho.c b-fb.c b-feph.c b-fro.c \
  b-grok.c b-isr.c b-kwxout.c b-peer.c \
  gnu-h-v.c \
  maketime.c merger.c partime.c rcsedit.c rcsfcmp.c rcsfnms.c \
  rcsgen.c rcskeep.c rcsmap.c rcsrev.c \
  rcstime.c rcsutil.c

executables = ident merge

bin_PROGRAMS = $(executables) grcs

BUILT_SOURCES = $(subs:=.help) $(executables:=.help) super.help

grcs_SOURCES = $(subs:=.c) super.c

LDADD = libparts.a -L$(top_builddir)/lib -lgnu

CLEANFILES = $(BUILT_SOURCES) $(subs)

EXTRA_DIST = sub.TEMPLATE

.c.help:
	$(top_srcdir)/build-aux/extract-help -o $@ $<

clean-local:
	rm -rf RCS
	rm -f a.*

# Makefile.am ends here

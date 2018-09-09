# Validate RCS behavior.

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

check_PROGRAMS = btdt

# If we use ‘btdt_CPPFLAGS’ here instead of ‘AM_CPPFLAGS’,
# Automake creates btdt-btdt.o instead of simply btdt.o.
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/lib
btdt_LDADD = ../src/libparts.a ../lib/libgnu.a

# This helps certain RCS programs find their peers.
PATHPREFIX = $(abs_top_builddir)/src

TESTS_ENVIRONMENT = 				\
  PATHPREFIX="$(PATHPREFIX)" sh

XFAIL_TESTS =

TESTS = \
 t010 \
 t030 \
 t050 \
 t150 \
 t151 \
 t153 \
 t160 \
 t180 \
 t300 \
 t310 \
 t311 \
 t312 \
 t313 \
 t320 \
 t370 \
 t380 \
 t390 \
 t391 \
 t400 \
 t410 \
 t420 \
 t430 \
 t440 \
 t450 \
 t460 \
 t470 \
 t510 \
 t511 \
 t600 \
 t601 \
 t602 \
 t603 \
 t604 \
 t605 \
 t606 \
 t607 \
 t608 \
 t609 \
 t620 \
 t630 \
 t780 \
 t790 \
 t800 \
 t801 \
 t802 \
 t810 \
 t900 \
 t999

EXTRA_DIST = $(TESTS) \
 common common-d common-i common-kn \
 compgate \
 fake

installcheck-local:
	$(MAKE) check PATHPREFIX='$(DESTDIR)$(bindir)'

clean-local:
	rm -rf *.d

describe:
	@for f in $(TESTS) ; do			\
	  sed 's/^...//;s/ ---/:/;q' $$f ;	\
	  if [ x"$(VERBOSE)" = x1 ] ; then	\
	    echo ;				\
	    sed -e '/^##$$/,/^##$$/!d'		\
	        -e 's/^.//'			\
	        -e 's/^#$$//'			\
	        -e '/./,/^$$/!d'		\
	        -e 's/^./    &/'		\
	        $$f ;				\
	  fi ;					\
	done

# Makefile.am ends here

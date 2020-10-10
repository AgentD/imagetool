ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

noinst_LIBRARIES =
noinst_PROGRAMS =
bin_PROGRAMS =
dist_man1_MANS =
check_PROGRAMS =
check_SCRIPTS =
pkgconfig_DATA =

EXTRA_DIST = autogen.sh README.md
TESTS =

include lib/fstream/Makemodule.am
include lib/tar/Makemodule.am
include lib/util/Makemodule.am
include lib/image/Makemodule.am
include lib/fstree/Makemodule.am
include lib/filesystem/Makemodule.am
include bin/imagebuild/Makemodule.am
include tests/Makemodule.am

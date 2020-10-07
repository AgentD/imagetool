AC_PREREQ([2.60])
AC_INIT([imgtool], [0.1], [goliath@infraroot.at], imgtool)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_RANLIB
AC_PROG_CC([cc gcc clang])
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_SED
AC_SYS_LARGEFILE

m4_ifndef([PKG_PROG_PKG_CONFIG],
  [m4_fatal([Could not locate the pkg-config autoconf
    macros. These are usually located in /usr/share/aclocal/pkg.m4.
    If your macros are in a different location, try setting the
    environment variable AL_OPTS="-I/other/macro/dir" before running
    ./autogen.sh or autoreconf again. Make sure pkg-config is installed.])])
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

UL_WARN_ADD([-Wall])
UL_WARN_ADD([-Wextra])
UL_WARN_ADD([-Wunused])
UL_WARN_ADD([-Wmissing-prototypes])
UL_WARN_ADD([-Wmissing-declarations])
UL_WARN_ADD([-Wwrite-strings])
UL_WARN_ADD([-Wjump-misses-init])
UL_WARN_ADD([-Wuninitialized])
UL_WARN_ADD([-Winit-self])
UL_WARN_ADD([-Wlogical-op])
UL_WARN_ADD([-Wunused-but-set-parameter])
UL_WARN_ADD([-Wunused-but-set-variable])
UL_WARN_ADD([-Wunused-parameter])
UL_WARN_ADD([-Wunused-result])
UL_WARN_ADD([-Wunused-variable])
UL_WARN_ADD([-Wduplicated-cond])
UL_WARN_ADD([-Wduplicated-branches])
UL_WARN_ADD([-Wrestrict])
UL_WARN_ADD([-Wnull-dereference])
UL_WARN_ADD([-pedantic])

AC_SUBST([WARN_CFLAGS])

##### config options #####

AC_ARG_WITH([bzip2],
	[AS_HELP_STRING([--with-bzip2],
			[Build with bzip2 compression support])],
	[], [with_bzip2="check"])

AC_ARG_WITH([xz],
	[AS_HELP_STRING([--with-xz], [Build with xz compression support])],
	[], [with_xz="check"])

AC_ARG_WITH([lzo],
	[AS_HELP_STRING([--with-lzo], [Build with lzo compression support])],
	[], [with_lzo="check"])

AC_ARG_WITH([lz4],
	[AS_HELP_STRING([--with-lz4], [Build with lz4 compression support])],
	[], [with_lz4="check"])

AC_ARG_WITH([zstd],
	[AS_HELP_STRING([--with-zstd], [Build with zstd compression support])],
	[], [with_zstd="check"])

AC_ARG_WITH([gzip],
	[AS_HELP_STRING([--with-gzip], [Build with zlib compression support])],
	[], [with_gzip="check"])

##### search for dependencies #####

AS_IF([test "x$with_bzip2" != "xno"], [
	PKG_CHECK_MODULES(BZIP2, [bzip2], [with_bzip2="yes"],
			      [AS_IF([test "x$with_bzip2" != "xcheck"],
				     [AC_MSG_ERROR([cannot Bzip2 libs])],
				     [with_bzip2="no"])])
], [])

AS_IF([test "x$with_gzip" != "xno"], [
	PKG_CHECK_MODULES(ZLIB, [zlib], [with_gzip="yes"],
				[AS_IF([test "x$with_gzip" != "xcheck"],
				       [AC_MSG_ERROR([cannot find zlib])],
				       [with_gzip="no"])])
], [])

AS_IF([test "x$with_xz" != "xno"], [
	PKG_CHECK_MODULES(XZ, [liblzma >= 5.0.0], [with_xz="yes"],
			      [AS_IF([test "x$with_xz" != "xcheck"],
				     [AC_MSG_ERROR([cannot XZ sdk])],
				     [with_xz="no"])])
], [])

AS_IF([test "x$with_lz4" != "xno"], [
	PKG_CHECK_MODULES(LZ4, [liblz4], [with_lz4="yes"],
			       [AS_IF([test "x$with_lz4" = "xyes"],
				      [AC_MSG_ERROR([cannot find liblz4])],
				      [with_lz4="no"])])
], [])

AS_IF([test "x$with_zstd" != "xno"], [
	PKG_CHECK_MODULES(ZSTD, [libzstd >= 1.3.1], [with_zstd="yes"],
				[AS_IF([test "x$with_zstd" = "xyes"],
				       [AC_MSG_ERROR([cannot find zstd])],
				       [with_zstd="no"])])
], [])

AS_IF([test "x$with_zstd" != "xno"], [AC_TEST_ZSTD_STREAM], [])

AC_ARG_VAR([LZO_CFLAGS], [C compiler flags for lzo])
AC_ARG_VAR([LZO_LIBS], [linker flags for lzo])

AS_IF([test -z "$LZO_LIBS" -a "x$with_lzo" != "xno"], [
	AC_CHECK_LIB([lzo2], [lzo1x_1_15_compress], [LZO_LIBS="-llzo2"],
			     [AC_CHECK_LIB([lzo],[lzo1x_1_15_compress],
				[LZO_LIBS="-llzo"],
			     	[]
			     )]
		    )
], [])

AS_IF([test -z "$LZO_LIBS"], [AS_IF([test "x$with_lzo" = "xyes"],
				    [AC_MSG_ERROR([cannot find liblzo])],
				    [with_lzo="no"])],
			     [with_lzo="yes"])

AM_CONDITIONAL([WITH_BZIP2], [test "x$with_bzip2" = "xyes"])
AM_CONDITIONAL([WITH_GZIP], [test "x$with_gzip" = "xyes"])
AM_CONDITIONAL([WITH_XZ], [test "x$with_xz" = "xyes"])
AM_CONDITIONAL([WITH_LZ4], [test "x$with_lz4" = "xyes"])
AM_CONDITIONAL([WITH_ZSTD], [test "x$with_zstd" = "xyes"])
AM_CONDITIONAL([WITH_LZO], [test "x$with_lzo" = "xyes"])

##### additional checks #####

AX_COMPILE_CHECK_SIZEOF(size_t)
AX_COMPILE_CHECK_SIZEOF(int)
AX_COMPILE_CHECK_SIZEOF(long)
AX_COMPILE_CHECK_SIZEOF(long long)

##### generate output #####

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT([Makefile])

AC_MSG_RESULT([
	${PACKAGE}  ${VERSION}

	prefix:            ${prefix}
	exec prefix:       ${exec_prefix}

	runstatedir:       ${runstatedir}
	bindir:            ${bindir}
	sbindir:           ${sbindir}
	libdir:            ${libdir}
	includedir:        ${includedir}

	compiler:          ${CC}
	cflags:            ${CFLAGS}
	ldflags:           ${LDFLAGS}

	GZIP support:      ${with_gzip}
	XZ/LZMA support:   ${with_xz}
	LZO support:       ${with_lzo}
	LZ4 support:       ${with_lz4}
	ZSTD support:      ${with_zstd}
	BZIP2 support:     ${with_bzip2}

	warnings:

${WARN_CFLAGS}

	Type 'make' or 'make <utilname>' to compile.
])

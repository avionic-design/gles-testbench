AC_PREREQ(2.59)
AC_INIT([gles-standalone], 0.1)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([no-dist-gzip dist-xz foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(GLESV2, glesv2)
PKG_CHECK_MODULES(EGL, egl)

CFLAGS="$CFLAGS -Wall"

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror],
		[Treat warnings as errors (default: disabled)])],
		[enable_werror="$enableval"],
		[enable_werror=no])
if test "x$enable_werror" = "xyes"; then
	CFLAGS="$CFLAGS -Werror"
fi

AC_OUTPUT([
	Makefile
	src/Makefile
])

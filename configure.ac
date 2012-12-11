AC_INIT([gles-standalone], 0.1)
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/gles-standalone.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC

AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
	AC_MSG_ERROR([You need to have pkg-config installed or set the PATH.])
])

PKG_CHECK_MODULES(GLIB, [
	glib-2.0
], [
	AC_SUBST(GLIB_CFLAGS)
	AC_SUBST(GLIB_LIBS)
], [
	AC_MSG_ERROR([
		Can't find glib-2.0.
	])
])

PKG_CHECK_MODULES(GIO, [
	gio-2.0
], [
	AC_SUBST(GIO_CFLAGS)
	AC_SUBST(GIO_LIBS)
], [
	AC_MSG_ERROR([
		Can't find gio-2.0.
	])
])

PKG_CHECK_MODULES(X11, [
	x11
], [
	AC_SUBST(X11_CFLAGS)
	AC_SUBST(X11_LIBS)
], [
	AC_MSG_ERROR([
		Can't find libX11.
	])
])

PKG_CHECK_MODULES(GLESV2, [
	glesv2
], [
	AC_SUBST(GLESV2_CFLAGS)
	AC_SUBST(GLESV2_LIBS)
], [
	AC_MSG_ERROR([
		Can't find GLESv2.
	])
])

PKG_CHECK_MODULES(EGL, [
	egl
], [
	AC_SUBST(EGL_CFLAGS)
	AC_SUBST(EGL_LIBS)
], [
	AC_MSG_ERROR([
		Can't find EGL.
	])
])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT

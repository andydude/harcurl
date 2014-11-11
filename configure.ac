m4_define([harcurl_version], m4_esyscmd([cat VERSION | xargs echo -n]))
m4_define([get_major_version], [m4_esyscmd([echo $1 | cut -d. -f1])])
m4_define([get_minor_version], [m4_esyscmd([echo $1 | cut -d. -f2])])
m4_define([get_micro_version], [m4_esyscmd([echo $1 | cut -d. -f3])])
AC_PREREQ(2.69)

AC_INIT([harcurl], [harcurl_version])
AC_CONFIG_AUX_DIR([config.aux])
AC_CONFIG_MACRO_DIR([aclocal.d])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])

# Checks for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_CC_STDC
AM_PROG_AR
AM_PATH_GLIB_2_0
AM_SILENT_RULES([yes])
LT_INIT

# Compiler flags
HAR_CFLAGS="$GOBJECT_CFLAGS"
HAR_LIBS="$GOBJECT_LIBS"
AC_SUBST(HAR_CFLAGS)
AC_SUBST(HAR_LIBS)

# Versioning
HARCURL_MAJOR_VERSION=get_major_version(harcurl_version)
HARCURL_MINOR_VERSION=get_minor_version(harcurl_version)
HARCURL_MICRO_VERSION=get_micro_version(harcurl_version)
HARCURL_VERSION=harcurl_version
AC_SUBST(HARCURL_MAJOR_VERSION)
AC_SUBST(HARCURL_MINOR_VERSION)
AC_SUBST(HARCURL_MICRO_VERSION)
AC_SUBST(HARCURL_VERSION)

# Dependancies
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([JANSSON], [jansson])
PKG_CHECK_MODULES([ZLIB], [zlib])

# Output
AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT

AC_INIT([Rumble Mail Server], 0.47)
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_ARG_WITH([MYSQL], AS_HELP_STRING([--with-mysql], [Build with MySQL support]))

my_save_cflags="$CFLAGS"
CFLAGS=-m64
AC_MSG_CHECKING([whether CC supports -m64])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    [AM_CFLAGS=-m64],
    [AC_MSG_RESULT([no])]
)
CFLAGS="$my_save_cflags"
AC_SUBST([AM_CFLAGS])

PKG_CHECK_MODULES(SQLITE, sqlite3 >= 3.2)
PKG_CHECK_MODULES(LUA, lua5.1 >= 5.1.2)
PKG_CHECK_MODULES(GNUTLS, gnutls >= 2.0)

if test "x$with_mysql" = "xyes"; then
   PKG_CHECK_MODULES([MYSQL], [mysql-client])
fi

AC_CHECK_LIB(pthread, pthread_create)
AC_CHECK_LIB(gnutls, gnutls_init)
AC_CHECK_LIB(sqlite3, sqlite3_open)
AC_CHECK_LIB(lua5.1, luaL_newstate)
AC_CHECK_LIB(resolv, ns_parserr)
AC_CHECK_LIB(gcrypt, gcry_control)


AC_PROG_RANLIB
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
AC_INIT([checkers], [0.1], [mustitz@gmail.com])
${CFLAGS=""}

AM_INIT_AUTOMAKE(dist-bzip2)
AC_CONFIG_MACRO_DIR([m4])



AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [enable debugging, default: no]),
    [case "${enableval}" in
        yes) debug=true ;;
        no)  debug=false ;;
        *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
    esac],
[debug=false])

AM_CONDITIONAL(DEBUG_MODE, test x"$debug" = x"true")



valgrind_withval=auto

AC_ARG_WITH([valgrind],
  AS_HELP_STRING([--with-valrgind], [Use valgrind to run testsuite]),
  [valgrind_withval=$withval]
)

AS_CASE(
  [$valgrind_withval],

  [no],
  [
    valgrind_withval=
  ],

  [yes],
  [
    AC_CHECK_PROG([have_valgrind], [valgrind], [yes], [no])
    AS_IF(
      [test "x$have_valgrind" == "xyes"],
      [valgrind_withval=valgrind],
      [AC_MSG_ERROR([Valgrind not found in PATH.])])
  ],
  
  [auto],
  [
    AC_CHECK_PROG([have_valgrind], [valgrind], [yes], [no])
    AS_IF(
      [test "x$have_valgrind" == "xyes"], 
      [valgrind_withval=valgrind],
      [valgrind_withval=])
  ],

  [
    AC_CHECK_FILE([$valgrind_withval], [have_valgrind=yes], [have_valgrind=no])
    AS_IF(
      [test "x$have_valgrind" != "xyes"],
      [
        valgrind_withval=
        AC_MSG_ERROR([Valgrind not found in PATH.])
      ])
  ]
)

AC_SUBST([VALGRIND], [$valgrind_withval])



AC_PROG_CC_C99
AM_SILENT_RULES([yes])



AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile source/Makefile include/Makefile validation/Makefile])
AC_CONFIG_FILES([validation/validation.sh], [chmod +x validation/validation.sh])


AC_OUTPUT

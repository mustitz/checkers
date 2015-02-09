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



AC_SUBST([VALGRIND], [valgrind])
valgrind_withval=auto
use_valgrind=false

AC_ARG_WITH([valgrind],
  AS_HELP_STRING([--with-valrgind], [Use valgrind to run testsuite]),
  [valgrind_withval=$withval]
)

AS_CASE(
  [$valgrind_withval],

  [no],
  [
  ],

  [yes],
  [
    AC_CHECK_PROG([have_valgrind], [$VALGRIND], [yes], [no])
    AS_IF(
      [test "x$have_valgrind" == "xyes"],
      [use_valgrind=true],
      [AC_MSG_ERROR([Valgrind not found in PATH.])])
  ],
  
  [auto],
  [
    AC_CHECK_PROG([have_valgrind], [$VALGRIND], [yes], [no])
    AS_IF(
      [test "x$have_valgrind" == "xyes"], 
      [use_valgrind=true])
  ],

  [
    AC_SUBST([VALGRIND], [$valgrind_withval])
    AC_CHECK_FILE([$VALGRIND], [have_valgrind=yes], [have_valgrind=no])
    AS_IF(
      [test "x$have_valgrind" == "xyes"],
      [use_valgrind=true],
      [AC_MSG_ERROR([Valgrind not found in PATH.])])
  ]
)



AC_PROG_CC_C99
AM_SILENT_RULES([yes])



AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile source/Makefile include/Makefile validation/Makefile])
AC_CONFIG_FILES([validation/run-insider], [chmod +x validation/run-insider])


AC_OUTPUT

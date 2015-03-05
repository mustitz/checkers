LOG_DRIVER = ./validation.sh

check_PROGRAMS = insider

insider_CFLAGS = -I../include
insider_SOURCES = insider.c

IO_TESTS = 001-empty.io 002-start-fen.io
TESTS = run-insider $(IO_TESTS)

EXTRA_DIST = $(IO_TESTS)
.PHONY : run-insider

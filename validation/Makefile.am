LOG_DRIVER = ./validation.sh

check_PROGRAMS = insider

insider_CFLAGS = -I${srcdir}/../include
insider_SOURCES = insider.c

IO_TESTS = \
	001-empty.io \
	002-start-fen.io \
	003-simple-moves.io \
	004-black-s-moves.io \
	005-ws-moves-complex.io \
	006-bs-moves-complex.io \
	007-ws-take.io \
	008-bs-take.io \
	009-ws-long-take.io \
	010-bs-long-take.io \
	011-ws-should-take.io \
	012-bs-should-take.io \
	013-ws-two-takes.io \
	014-bs-two-takes.io \
	015-ws-max-long-take.io \
	016-bs-max-long-take.io \
	017-wm-moves.io \
	018-bm-moves.io \
	019-wm-lock.io \
	020-bm-lock.io \
	021-wm-simple-complex.io \
	022-bm-simple-complex.io \
	023-wm-take.io \
	024-bm-take.io \
	025-wm-long-take.io \
	026-bm-long-take.io \
	027-wm-take-crazy.io \
	028-bm-take-crazy.io \
	029-petrov-ziric-game.io \
	030-ziric-flikstein-game.io


	
TESTS = run-insider $(IO_TESTS)

EXTRA_DIST = $(IO_TESTS)
.PHONY : run-insider

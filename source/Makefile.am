bin_PROGRAMS = rus-checkers
noinst_PROGRAMS = gen-magic



if DEBUG_MODE
EXTRA_CFLAGS = -g3 -O0 -Wall -Werror
else
EXTRA_CFLAGS = -Ofast
endif

rus_checkers_CFLAGS = $(EXTRA_CFLAGS)
rus_checkers_SOURCES = main.c
nodist_rus_checkers_SOURCES = magic-tables.c

gen_magic_CFLAGS = $(EXTRA_CFLAGS)
gen_magic_SOURCES = gen-magic.c


magic-tables.c : gen-magic
	./gen-magic > magic-tables.c

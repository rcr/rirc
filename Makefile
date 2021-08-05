VERSION := 0.1.5

PREFIX   ?= /usr/local
PATH_BIN := $(DESTDIR)$(PREFIX)/bin
PATH_MAN := $(DESTDIR)$(PREFIX)/share/man/man1

PATH_BUILD := build
PATH_LIB   := lib
PATH_SRC   := src
PATH_TEST  := test

include lib/mbedtls.Makefile

CONFIG := config.h

CFLAGS_RIRC += -std=c11 -I. -DVERSION=\"$(VERSION)\"
CFLAGS_RIRC += -D_POSIX_C_SOURCE=200809L
CFLAGS_RIRC += -D_DARWIN_C_SOURCE

CFLAGS ?= -O2 -flto
CFLAGS += -DNDEBUG

CFLAGS_DEBUG += -O0 -g3 -Wall -Wextra -Werror

LDFLAGS ?= -flto
LDFLAGS += -pthread

LDFLAGS_DEBUG += -lpthread

SRC       := $(shell find $(PATH_SRC) -name '*.c' | sort)
SRC_GPERF := $(patsubst %, %.out, $(shell find $(PATH_SRC) -name '*.gperf'))

# Release objects, debug objects, testcases
OBJS_R := $(patsubst $(PATH_SRC)/%.c, $(PATH_BUILD)/%.o,    $(SRC))
OBJS_D := $(patsubst $(PATH_SRC)/%.c, $(PATH_BUILD)/%.db.o, $(SRC))
OBJS_T := $(patsubst $(PATH_SRC)/%.c, $(PATH_BUILD)/%.t,    $(SRC))
OBJS_T += $(PATH_BUILD)/utils/tree.t # Header only file

rirc: $(RIRC_LIBS) $(SRC_GPERF) $(OBJS_R)
	@echo "$(CC) $(LDFLAGS) $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R) $(RIRC_LIBS)

rirc.debug: $(RIRC_LIBS) $(SRC_GPERF) $(OBJS_D)
	@echo "$(CC) $(LDFLAGS_DEBUG) $@"
	@$(CC) $(LDFLAGS_DEBUG) -o $@ $(OBJS_D) $(RIRC_LIBS)

$(PATH_BUILD)/%.o: $(PATH_SRC)/%.c $(CONFIG) | $(PATH_BUILD)
	@echo "$(CC) $(CFLAGS) $<"
	@$(CPP) $(CFLAGS) $(CFLAGS_RIRC) -MM -MP -MT $@ -MF $(@:.o=.o.d) $<
	@$(CC)  $(CFLAGS) $(CFLAGS_RIRC) -c -o $@ $<

$(PATH_BUILD)/%.db.o: $(PATH_SRC)/%.c $(CONFIG) | $(PATH_BUILD)
	@echo "$(CC) $(CFLAGS_DEBUG) $<"
	@$(CPP) $(CFLAGS_DEBUG) $(CFLAGS_RIRC) -MM -MP -MT $@ -MF $(@:.o=.o.d) $<
	@$(CC)  $(CFLAGS_DEBUG) $(CFLAGS_RIRC) -c -o $@ $<

$(PATH_BUILD)/%.t: $(PATH_TEST)/%.c $(SRC_GPERF) $(CONFIG) | $(RIRC_LIBS) $(PATH_BUILD)
	@$(CPP) $(CFLAGS_DEBUG) $(CFLAGS_RIRC) -MM -MP -MT $@ -MF $(@:.t=.t.d) $<
	@$(CC)  $(CFLAGS_DEBUG) $(CFLAGS_RIRC) -c -o $(@:.t=.t.o) $<
	@$(CC)  $(LDFLAGS_DEBUG) -o $@ $(@:.t=.t.o) $(RIRC_LIBS)
	@{ rm -f $(@:.t=.td) && ./$@; } || mv $@ $(@:.t=.td)

$(PATH_BUILD):
	@mkdir -p $(patsubst $(PATH_SRC)%, $(PATH_BUILD)%, $(shell find $(PATH_SRC) -type d))

$(CONFIG):
	cp config.def.h config.h

%.gperf.out: %.gperf
	gperf --output-file=$@ $<

all:
	@$(MAKE) --silent rirc
	@$(MAKE) --silent rirc.debug

check: $(OBJS_T)
	@[ ! "$$(find $(PATH_BUILD) -name '*.td' -print -quit)" ] && echo OK

clean:
	@rm -rfv rirc rirc.debug $(SRC_GPERF) $(PATH_BUILD)

libs:
	@$(MAKE) --silent $(RIRC_LIBS)

install: rirc
	@sed "s/VERSION/$(VERSION)/g" < docs/rirc.1 > rirc.1
	mkdir -p $(PATH_BIN)
	mkdir -p $(PATH_MAN)
	cp -f rirc   $(PATH_BIN)
	cp -f rirc.1 $(PATH_MAN)
	chmod 755 $(PATH_BIN)/rirc
	chmod 644 $(PATH_MAN)/rirc.1

uninstall:
	rm -f $(PATH_BIN)/rirc
	rm -f $(PATH_MAN)/rirc.1

-include $(OBJS_R:.o=.o.d)
-include $(OBJS_D:.o=.o.d)
-include $(OBJS_T:.t=.t.d)

.DEFAULT_GOAL := rirc

.PHONY: all check clean libs install uninstall

.SUFFIXES:

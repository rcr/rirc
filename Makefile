VERSION := 0.1.4

# Release and debug executable names
BIN_R := rirc
BIN_D := rirc.debug

# Install paths
BIN_DIR := /usr/local/bin
MAN_DIR := /usr/local/share/man/man1

# Build, source, lib, and test directories
DIR_B := bld
DIR_S := src
DIR_L := lib
DIR_T := test

CONFIG := config.h

include lib/mbedtls.Makefile

CFLAGS ?= -O2 -flto
CFLAGS += -D NDEBUG

CFLAGS_DEBUG ?=
CFLAGS_DEBUG += -O0 -g3

CFLAGS_LOCAL := -std=c11 -Wall -Wextra -Werror -D VERSION=\"$(VERSION)\"
CFLAGS_LOCAL += -I . -I $(MBEDTLS_SRC)/include/
CFLAGS_LOCAL += -D MBEDTLS_CONFIG_FILE='<$(MBEDTLS_CFG)>'
CFLAGS_LOCAL += -D _POSIX_C_SOURCE=200809L
CFLAGS_LOCAL += -D _DARWIN_C_SOURCE

LDFLAGS ?=
LDFLAGS += -lpthread

SRC     := $(shell find $(DIR_S) -name '*.c')
SUBDIRS += $(shell find $(DIR_S) -name '*.c' -exec dirname {} \; | sort -u)

SRC_G   := $(shell find $(DIR_S) -name '*.gperf')
SUBDIRS += $(shell find $(DIR_S) -name '*.gperf' -exec dirname {} \; | sort -u)

# Release, debug, and testcase build objects
OBJS_D := $(patsubst $(DIR_S)/%.c, $(DIR_B)/%.db.o, $(SRC))
OBJS_R := $(patsubst $(DIR_S)/%.c, $(DIR_B)/%.o,    $(SRC))
OBJS_T := $(patsubst $(DIR_S)/%.c, $(DIR_B)/%.t,    $(SRC))
OBJS_T += $(DIR_B)/utils/tree.t # Header only file

# Gperf generated source files
OBJS_G := $(patsubst %.gperf, %.gperf.out, $(SRC_G))

# Release build executable
$(BIN_R): $(MBEDTLS_LIBS) $(OBJS_G) $(OBJS_R)
	@echo "  CC    $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R) $(MBEDTLS_LIBS)

# Debug build executable
$(BIN_D): $(MBEDTLS_LIBS) $(OBJS_G) $(OBJS_D)
	@echo "  CC    $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_D) $(MBEDTLS_LIBS)

# Release build objects
$(DIR_B)/%.o: $(DIR_S)/%.c $(CONFIG) | $(DIR_B)
	@echo "  CC    $<"
	@$(CPP) $(CFLAGS) $(CFLAGS_LOCAL) -MM -MP -MT $@ -MF $(@:.o=.o.d) $<
	@$(CC)  $(CFLAGS) $(CFLAGS_LOCAL) -c -o $@ $<

# Debug build objects
$(DIR_B)/%.db.o: $(DIR_S)/%.c $(CONFIG) | $(DIR_B)
	@echo "  CC    $<"
	@$(CPP) $(CFLAGS_DEBUG) $(CFLAGS_LOCAL) -MM -MP -MT $@ -MF $(@:.o=.o.d) $<
	@$(CC)  $(CFLAGS_DEBUG) $(CFLAGS_LOCAL) -c -o $@ $<

# Testcases
$(DIR_B)/%.t: $(DIR_T)/%.c $(OBJS_G) $(CONFIG) | $(DIR_B)
	@$(CPP) $(CFLAGS_DEBUG) $(CFLAGS_LOCAL) -MM -MP -MT $@ -MF $(@:.t=.t.d) $<
	@$(CC)  $(CFLAGS_DEBUG) $(CFLAGS_LOCAL) -c -o $(@:.t=.t.o) $<
	@$(CC)  $(CFLAGS_DEBUG) $(CFLAGS_LOCAL) -o $@ $(@:.t=.t.o)
	@./$@ || mv $@ $(@:.t=.td)
	@[ -f $@ ]

# Build directories
$(DIR_B):
	@for dir in $(patsubst $(DIR_S)/%, $(DIR_B)/%, $(SUBDIRS)); do mkdir -p $$dir; done

$(CONFIG):
	cp config.def.h config.h

%.gperf.out: %.gperf
	gperf --output-file=$@ $<

all:
	@$(MAKE) --silent $(BIN_R)
	@$(MAKE) --silent $(BIN_D)

check:
	@$(MAKE) --silent $(OBJS_T)

clean:
	@rm -rf $(DIR_B)
	@rm -vf $(BIN_R) $(BIN_D) $(OBJS_G)

libs:
	@$(MAKE) --silent $(MBEDTLS_LIBS)

install: $(BIN_R)
	@echo installing executable to $(BIN_DIR)
	@echo installing manual page to $(MAN_DIR)
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(MAN_DIR)
	@cp -f rirc $(BIN_DIR)
	@chmod 755 $(BIN_DIR)/rirc
	@sed "s/VERSION/$(VERSION)/g" < rirc.1 > $(MAN_DIR)/rirc.1

uninstall:
	rm -f $(BIN_DIR)/rirc
	rm -f $(MAN_DIR)/rirc.1

-include $(OBJS_R:.o=.o.d)
-include $(OBJS_D:.o=.o.d)
-include $(OBJS_T:.t=.t.d)

.DEFAULT_GOAL := $(BIN_R)

.PHONY: all check clean libs install uninstall

.SUFFIXES:

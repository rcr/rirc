VERSION := 0.1.3

# Release and debug build executable names
BIN_R := rirc
BIN_D := rirc.debug

# Install paths
BIN_DIR := /usr/local/bin
MAN_DIR := /usr/local/share/man/man1

TLS_CONF := $(PWD)/lib/mbedtls.h
TLS_INCL := -I $(PWD)/lib/mbedtls/include/ -DMBEDTLS_CONFIG_FILE='<$(TLS_CONF)>'
TLS_LIBS := ./lib/mbedtls/library/libmbedtls.a \
            ./lib/mbedtls/library/libmbedx509.a \
            ./lib/mbedtls/library/libmbedcrypto.a

STDS := -std=c11 -D_POSIX_C_SOURCE=200809L

CC := cc
PP := cc -E
CFLAGS   := $(CC_EXT) $(STDS) $(TLS_INCL) -I. -DVERSION=\"$(VERSION)\" -Wall -Wextra -pedantic
CFLAGS_R := $(CFLAGS) -O2 -flto -DNDEBUG
CFLAGS_D := $(CFLAGS) -O0 -g
LDFLAGS  := $(LD_EXT) -lpthread

# Build, source, test source directories
DIR_B := bld
DIR_S := src
DIR_T := test

PWD  := $(shell pwd)

SRC     := $(shell find $(DIR_S) -name '*.c')
SUBDIRS += $(shell find $(DIR_S) -name '*.c' -exec dirname {} \; | sort -u)

SRC_G   := $(shell find $(DIR_S) -name '*.gperf')
SUBDIRS += $(shell find $(DIR_S) -name '*.gperf' -exec dirname {} \; | sort -u)

# Release, debug, testcase build objects
OBJS_D := $(patsubst $(DIR_S)/%.c, $(DIR_B)/%.db.o, $(SRC))
OBJS_R := $(patsubst $(DIR_S)/%.c, $(DIR_B)/%.o,    $(SRC))
OBJS_T := $(patsubst $(DIR_S)/%.c, $(DIR_B)/%.t,    $(SRC))
OBJS_T += $(DIR_B)/utils/tree.t # Header only file

# Gperf generated source files
OBJS_G := $(patsubst %.gperf, %.gperf.out, $(SRC_G))

# Release build executable
$(BIN_R): libs $(OBJS_G) $(OBJS_R)
	@echo " CC  $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R) $(TLS_LIBS)

# Debug build executable
$(BIN_D): libs $(OBJS_G) $(OBJS_D)
	@echo " CC  $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_D) $(TLS_LIBS)

# Release build objects
$(DIR_B)/%.o: $(DIR_S)/%.c config.h | $(DIR_B)
	@echo " CC  $<"
	@$(PP) $(CFLAGS_R) -MM -MP -MT $@ -MF $(@:.o=.o.d) $<
	@$(CC) $(CFLAGS_R) -c -o $@ $<

# Debug build objects
$(DIR_B)/%.db.o: $(DIR_S)/%.c config.h | $(DIR_B)
	@echo " CC  $<"
	@$(PP) $(CFLAGS_D) -MM -MP -MT $@ -MF $(@:.o=.o.d) $<
	@$(CC) $(CFLAGS_D) -c -o $@ $<

# Testcases
$(DIR_B)/%.t: $(DIR_T)/%.c $(OBJS_G) | $(DIR_B)
	@$(PP) $(CFLAGS_D) -MM -MP -MT $@ -MF $(@:.t=.t.d) $<
	@$(CC) $(CFLAGS_D) -c -o $(@:.t=.t.o) $<
	@$(CC) $(CFLAGS_D) -o $@ $(@:.t=.t.o)
	@$(TEST_EXT) ./$@

config.h:
	cp config.def.h config.h

%.gperf.out: %.gperf
	gperf --output-file=$@ $<

# Build directories
$(DIR_B):
	@for dir in $(patsubst $(DIR_S)/%, %, $(SUBDIRS)); do mkdir -p $(DIR_B)/$$dir; done

# TLS libraries
$(TLS_LIBS): $(TLS_CONF)
	@CFLAGS="$(TLS_INCL)" $(MAKE) --silent -C ./lib/mbedtls clean
	@CFLAGS="$(TLS_INCL)" $(MAKE) --silent -C ./lib/mbedtls lib

all: libs
	@$(MAKE) --silent $(BIN_R)
	@$(MAKE) --silent $(BIN_D)

check:
	@$(MAKE) --silent $(OBJS_T)

clean:
	@rm -rf $(DIR_B)
	@rm -vf $(BIN_R) $(BIN_D) $(OBJS_G)

libs:
	@$(MAKE) --silent $(TLS_LIBS)

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

.PHONY: all check clean libs install uninstall

.PRECIOUS: $(OBJS_T)

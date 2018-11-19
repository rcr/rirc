.POSIX:

VERSION = 0.1.1

# Release and debug build executable names
EXE_R := rirc
EXE_D := rirc.debug

# Install paths
EXE_DIR = /usr/local/bin
MAN_DIR = /usr/local/share/man/man1

STANDARDS = -std=c99 \
 -D_POSIX_C_SOURCE=200112L \
 -D_DARWIN_C_SOURCE=200112L \
 -D_BSD_VISIBLE=1

CC = cc
PP = cc -E
CFLAGS    = -I. $(STANDARDS) -DVERSION=\"$(VERSION)\" $(D_EXT) -Wall -Wextra -pedantic -O2 -flto
CFLAGS_D  = -I. $(STANDARDS) -DVERSION=\"$(VERSION)\" $(D_EXT) -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS   = $(L_EXT) -pthread
LDFLAGS_D = $(L_EXT) -pthread

# Build, source, test source directories
DIR_B := bld
DIR_S := src
DIR_T := test

SRC     := $(shell find $(DIR_S) -name '*.c')
SRCDIRS := $(shell find $(DIR_S) -name '*.c' -exec dirname {} \; | sort -u)

SRC_G     := $(shell find $(DIR_S) -name '*.gperf')
SRCDIRS_G := $(shell find $(DIR_S) -name '*.gperf' -exec dirname {} \; | sort -u)

SRC_T     := $(shell find $(DIR_T) -name '*.c')
SRCDIRS_T := $(shell find $(DIR_T) -name '*.c' -exec dirname {} \; | sort -u)

# Release, debug, testcase build objects
OBJS_R := $(patsubst %.c,  $(DIR_B)/%.o,    $(SRC))
OBJS_D := $(patsubst %.c,  $(DIR_B)/%.db.o, $(SRC))
OBJS_T := $(patsubst %.c,  $(DIR_B)/%.t,    $(SRC_T))

# Gperf generated source files
OBJS_G := $(patsubst %.gperf, %.gperf.h, $(SRC_G))

# Release build executable
$(EXE_R): $(DIR_B) $(OBJS_G) $(OBJS_R)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R)

# Debug build executable
$(EXE_D): $(DIR_B) $(OBJS_G) $(OBJS_D)
	@echo cc $@
	@$(CC) $(LDFLAGS_D) -o $@ $(OBJS_D)

# Release build objects
$(DIR_B)/%.o: %.c
	@echo "cc $<..."
	@$(PP) $(CFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	@$(CC) $(CFLAGS) -c -o $@ $<

# Debug build objects
$(DIR_B)/%.db.o: %.c
	@echo "cc $<..."
	@$(PP) $(CFLAGS_D) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	@$(CC) $(CFLAGS_D) -c -o $@ $<

# Gperf generated source
%.gperf.h: %.gperf
	gperf --output-file=$(@) $<

# Testcase files
$(DIR_B)/%.t: %.c
	@$(PP) $(CFLAGS_D) -MM -MP -MT $@ -MF $(@:.t=.d) $<
	@$(CC) $(CFLAGS_D) $(LDFLAGS_D) -o $@ $<
	-@./$@ || mv $@ $(@:.t=.td)

-include $(OBJS_R:.o=.d)
-include $(OBJS_D:.o=.d)
-include $(OBJS_T:.t=.d)

$(DIR_B):
	@$(call make-dirs)

default: $(EXE_R)
debug:   $(EXE_D)
test:    $(DIR_B) $(OBJS_G) $(OBJS_T)

clean:
	rm -rf $(DIR_B) $(EXE_R) $(EXE_D)

define make-dirs
	for dir in $(SRCDIRS) $(SRCDIRS_T) $(SRCDIRS_G); do mkdir -p $(DIR_B)/$$dir; done
endef

install: $(EXE_R)
	@echo installing executable to $(EXE_DIR)
	@echo installing manual page to $(MAN_DIR)
	@mkdir -p $(EXE_DIR)
	@mkdir -p $(MAN_DIR)
	@cp -f rirc $(EXE_DIR)
	@chmod 755 $(EXE_DIR)/rirc
	@sed "s/VERSION/$(VERSION)/g" < rirc.1 > $(MAN_DIR)/rirc.1

uninstall:
	rm -f $(EXE_DIR)/rirc
	rm -f $(MAN_DIR)/rirc.1

.PHONY: clean default install uninstall test

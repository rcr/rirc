.POSIX:

VERSION := 0.1.1

# Release and debug build executable names
EXE_R := rirc
EXE_D := rirc.debug

# Install paths
EXE_DIR = /usr/local/bin
MAN_DIR = /usr/local/share/man/man1

STDS := \
 -std=c99 \
 -D_POSIX_C_SOURCE=200112L \
 -D_DARWIN_C_SOURCE=200112L \
 -D_BSD_VISIBLE=1

CC := cc
PP := cc -E
CFLAGS   := $(CC_EXT) -I. $(STDS) -DVERSION=\"$(VERSION)\" -Wall -Wextra -pedantic -O2 -flto
CFLAGS_D := $(CC_EXT) -I. $(STDS) -DVERSION=\"$(VERSION)\" -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS  := $(LD_EXT) -pthread

# Build, source, test source directories
DIR_B := bld
DIR_S := src
DIR_T := test

SRC     := $(shell find $(DIR_S) -name '*.c')
SUBDIRS += $(shell find $(DIR_S) -name '*.c' -exec dirname {} \; | sort -u)

SRC_G   := $(shell find $(DIR_S) -name '*.gperf')
SUBDIRS += $(shell find $(DIR_S) -name '*.gperf' -exec dirname {} \; | sort -u)

# Release, debug, testcase build objects
OBJS_D := $(patsubst $(DIR_S)/%.c,  $(DIR_B)/%.db.o, $(SRC))
OBJS_R := $(patsubst $(DIR_S)/%.c,  $(DIR_B)/%.o,    $(SRC))
OBJS_T := $(patsubst $(DIR_S)/%.c,  $(DIR_B)/%.t,    $(SRC))

# Gperf generated source files
OBJS_G := $(patsubst %.gperf, %.gperf.out, $(SRC_G))

# Release build executable
$(EXE_R): $(DIR_B) $(OBJS_G) $(OBJS_R)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R)

# Debug build executable
$(EXE_D): $(DIR_B) $(OBJS_G) $(OBJS_D)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_D)

# Release build objects
$(DIR_B)/%.o: $(DIR_S)/%.c
	@echo "cc $<..."
	@$(PP) $(CFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	@$(CC) $(CFLAGS) -c -o $@ $<

# Debug build objects
$(DIR_B)/%.db.o: $(DIR_S)/%.c
	@echo "cc $<..."
	@$(PP) $(CFLAGS_D) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	@$(CC) $(CFLAGS_D) -c -o $@ $<

# Gperf generated source
%.gperf.out: %.gperf
	gperf --output-file=$(@) $<

# Testcase files
$(DIR_B)/%.t: $(DIR_T)/%.c
	@$(PP) $(CFLAGS_D) -MM -MP -MT $@ -MF $(@:.t=.d) $<
	@$(CC) $(CFLAGS_D) $(LDFLAGS) -o $@ $<
	-@./$@ || mv $@ $(@:.t=.td)

# Build directories
$(DIR_B):
	@for dir in $(patsubst $(DIR_S)/%, %, $(SUBDIRS)); do mkdir -p $(DIR_B)/$$dir; done

clean:
	rm -rf $(DIR_B) $(EXE_R) $(EXE_D)

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

all:   $(EXE_R)
debug: $(EXE_D)
test:  $(DIR_B) $(OBJS_G) $(OBJS_T)

-include $(OBJS_R:.o=.d)
-include $(OBJS_D:.o=.d)
-include $(OBJS_T:.t=.d)

.PHONY: all clean default install uninstall options test

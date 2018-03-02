CC = cc
PP = cc -E
CFLAGS    = -I. -std=c99 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -pedantic -O2
CFLAGS_D  = -I. -std=c99 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS   = -pthread -s
LDFLAGS_D = -pthread

# Build, source, test source directories
DIR_B := bld
DIR_S := src
DIR_T := test

SRC     := $(shell find $(DIR_S) -iname '*.c')
SRCDIRS := $(shell find $(DIR_S) -iname '*.c' -exec dirname {} \; | sort -u)

SRC_T     := $(shell find $(DIR_T) -iname '*.c')
SRCDIRS_T := $(shell find $(DIR_T) -iname '*.c' -exec dirname {} \; | sort -u)

# Release, debug, testcase objects
OBJS_R := $(patsubst %.c, $(DIR_B)/%.o,    $(SRC))
OBJS_D := $(patsubst %.c, $(DIR_B)/%.db.o, $(SRC))
OBJS_T := $(patsubst %.c, $(DIR_B)/%.t,    $(SRC_T))

# Release build, Debug build
EXE_R  := rirc
EXE_D  := rirc.debug

# Release build executable
$(EXE_R): $(DIR_B) $(OBJS_R)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R)

# Debug build executable
$(EXE_D): $(DIR_B) $(OBJS_D)
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
test:    $(DIR_B) $(OBJS_T)

clean:
	@echo cleaning
	@rm -rf $(DIR_B) $(EXE_R) $(EXE_D)

define make-dirs
	for dir in $(SRCDIRS);   do mkdir -p $(DIR_B)/$$dir; done
	for dir in $(SRCDIRS_T); do mkdir -p $(DIR_B)/$$dir; done
endef

.PHONY: clean debug default test

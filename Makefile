CC = cc
PP = cc -E
CFLAGS       = -std=c99 -Wall -Wextra -pedantic -O2
CFLAGS_DEBUG = -std=c99 -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS      = -pthread

BLDDIR := bld
SRCDIR := src

SRC     := $(shell find $(SRCDIR) -iname '*.c')
SRCDIRS := $(shell find $(SRCDIR) -iname '*.c' -exec dirname {} \; | sort -u)

# Relsease build, Debug build
OBJS_R := $(patsubst %.c, $(BLDDIR)/%.o, $(SRC))
OBJS_D := $(patsubst %.c, $(BLDDIR)/%.db.o, $(SRC))
EXE_R  := rirc
EXE_D  := rirc.debug

# Release build exe, objects
$(EXE_R): $(BLDDIR) $(OBJS_R)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_R)

$(BLDDIR)/%.o: %.c
	@echo "cc $<..."
	@$(PP) $(CFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	@$(CC) $(CFLAGS) -c -o $@ $<

# Debug build exe, objects
$(EXE_D): $(BLDDIR) $(OBJS_D)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_D)

$(BLDDIR)/%.db.o: %.c
	@echo "cc $<..."
	@$(PP) $(CFLAGS_DEBUG) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	@$(CC) $(CFLAGS_DEBUG) -c -o $@ $<

-include $(OBJS_R:.o=.d)
-include $(OBJS_D:.o=.d)

$(BLDDIR):
	@$(call make-dirs)

clean:
	@echo cleaning
	@rm -rf $(BLDDIR) $(EXE_R) $(EXE_D)

define make-dirs
	for dir in $(SRCDIRS); do mkdir -p $(BLDDIR)/$$dir; done
endef

default: $(EXE_R)
debug:   $(EXE_D)

.PHONY: clean debug default

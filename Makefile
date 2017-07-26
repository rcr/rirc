CC = cc
PP = cc -E
CFLAGS       = -std=c99 -Wall -Wextra -pedantic -O2
CFLAGS_DEBUG = -std=c99 -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS      = -pthread

# If using gcc for debug and sanitization, e.g.:
#   > make -e CC=gcc debug
ifeq ($(CC), gcc)
	CFLAGS_DEBUG += -fsanitize=undefined,address
	LDFLAGS_DEBUG = -pthread -lasan -lubsan
endif

SDIR = src
BDIR = src/bld

SDIR_T = test
BDIR_T = test/bld

# Source and build files
SRC = $(shell find $(SDIR) -iname '*.c')
BLD = $(shell echo '$(SRC:.c=.o)' | sed 's|$(SDIR)/|,|g; s|/|.|g; s|,|$(BDIR)/|g')

# Test source and build files
SRC_T = $(shell find $(SDIR_T) -iname '*.c')
BLD_T = $(shell echo '$(SRC_T:.c=.t)' | sed 's|$(SDIR_T)/|,|g; s|/|.|g; s|,|$(BDIR_T)/|g')

rirc: $(BLD)
	@echo cc $@
	@$(CC) $(LDFLAGS) -o $@ $^

$(BDIR)%.o:
	$(eval _SRC = $(SDIR)/$(shell echo '$(@F)' | sed 's|\.o$$||; s|\.|/|; s|$$|.c|'))
	@echo cc $(_SRC)
	@$(PP) $(CFLAGS) -MM -MP -MT $@ $(_SRC) -MF $(@:.o=.d)
	@$(CC) $(CFLAGS) -c -o $@ $(_SRC)

$(BDIR_T)%.t:
	$(eval _SRC = $(SDIR_T)/$(shell echo '$(@F)' | sed 's|\.t$$||; s|\.|/|; s|$$|.c|'))
	@$(PP) $(CFLAGS) -MM -MP -MT $@ $(_SRC) -MF $(@:.t=.d)
	@$(CC) $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) -o $@ $(_SRC)
	-@./$@ || mv $@ $(@:.t=.td)

-include $(wildcard $(BDIR)/*.d) $(wildcard $(BDIR_T)/*.d)

clean:
	@echo cleaning
	@rm -f rirc $(BDIR)/*{o,d} $(BDIR_T)/*.{t,d}

debug: CFLAGS   = $(CFLAGS_DEBUG)
debug: LDFLAGS += $(LDFLAGS_DEBUG)
debug: rirc

default: rirc

test: $(BLD_T)

.PHONY: clean debug default test

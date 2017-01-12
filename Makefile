CC = cc
CFLAGS       = -std=c11 -Wall -Wextra -pedantic -O2
CFLAGS_DEBUG = -std=c11 -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS      = -pthread

# If using gcc for debug and sanitization, e.g.:
#   > make -e CC=gcc debug
ifeq ($(CC), gcc)
	CFLAGS_DEBUG += -fsanitize=undefined,address
	LDFLAGS_DEBUG = -pthread -lasan -lubsan
endif

SRCDIR = src/
BLDDIR = src/bld/

SRCDIR_T = test/
BLDDIR_T = test/bld/

# Source and build files
SRC = $(wildcard $(SRCDIR)*.c)
OBJ = $(patsubst $(SRCDIR)%.c, $(BLDDIR)%.o, $(SRC))

# Test source and build files
SRC_T = $(wildcard $(SRCDIR_T)*.c)
OBJ_T = $(patsubst $(SRCDIR_T)%.c, $(BLDDIR_T)%.t, $(SRC_T))

rirc: $(OBJ)
	@echo $@
	@$(CC) $(LDFLAGS) -o $@ $^

$(BLDDIR)%.o: $(SRCDIR)%.c
	@echo $@
	@$(CPP) $(CFLAGS) -MM -MP -MT $@ $< -MF $(@:.o=.d)
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BLDDIR_T)%.t: $(SRCDIR_T)%.c
	@$(CPP) $(CFLAGS) -MM -MP -MT $@ $< -MF $(@:.t=.d)
	@$(CC) $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) -o $@ $<
	-@./$@ || rm $@

-include $(BLDDIR)*.d $(BLDDIR_T)*.d

clean:
	@echo cleaning
	@rm -f rirc $(BLDDIR)*.{o,d} $(BLDDIR_T)*.{t,d}

debug: CFLAGS   = $(CFLAGS_DEBUG)
debug: LDFLAGS += $(LDFLAGS_DEBUG)
debug: rirc

default: rirc

test: $(OBJ_T)

.PHONY: clean debug default test

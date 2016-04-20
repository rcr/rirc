CC = cc
CFLAGS       = -std=c99 -Wall -Wextra -pedantic -O2
CFLAGS_DEBUG = -std=c99 -Wall -Wextra -pedantic -O0 -g -DDEBUG
LDFLAGS      = -pthread

# If using gcc for debug and sanitization, eg:
#   > make -e CC=gcc debug
ifeq ($(CC),gcc)
	CFLAGS_DEBUG += -fsanitize=undefined,address
	LDFLAGS_DEBUG = -pthread -lubsan -lasan
endif

SDIR = src
TDIR = test

# Common header files
HDS = $(SDIR)/common.h $(SDIR)/config.h

# Source and object files
SRC = $(wildcard $(SDIR)/*.c)
OBJ = $(patsubst $(SDIR)%.c,$(SDIR_O)%.o,$(SRC))
SDIR_O = $(SDIR)/bld

# Test source and executable files
SRC_T = $(wildcard $(TDIR)/*.c)
OBJ_T = $(patsubst $(TDIR)%.c,$(TDIR_O)%.test,$(SRC_T))
TDIR_O = $(TDIR)/bld

default: rirc

$(SDIR)/config.h: config.def.h
	@echo creating $@ from config.def.h
	@cp config.def.h $@

rirc: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(SDIR_O)/%.o: $(SDIR)/%.c $(HDS)
	$(CC) $(CFLAGS) -c -o $@ $<

# Testcases link to math libs for some calculations
$(TDIR_O)/%.test: $(TDIR)/%.c
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) -lm -o $@ $<

test: clean $(OBJ_T)
	@for test in $(OBJ_T); do ./$$test; done

debug: CFLAGS   = $(CFLAGS_DEBUG)
debug: LDFLAGS += $(LDFLAGS_DEBUG)
debug: clean rirc

clean:
	@echo cleaning
	@rm -f rirc $(SDIR_O)/*.o $(TDIR_O)/*.test

.PHONY: clean debug default test

CC = cc
CCFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -pthread

SDIR = src
TDIR = test

# Common header files
HDS = $(SDIR)/common.h

# Source and object files
SRC = $(wildcard $(SDIR)/*.c)
OBJ = $(patsubst $(SDIR)%.c,$(SDIR_O)%.o,$(SRC))
SDIR_O = $(SDIR)/bld

# Test source and executable files
SRC_T = $(wildcard $(TDIR)/*.c)
OBJ_T = $(patsubst $(TDIR)%.c,$(TDIR_O)%.test,$(SRC_T))
TDIR_O = $(TDIR)/bld

rirc: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(SDIR_O)/%.o: $(SDIR)/%.c $(HDS)
	$(CC) $(CCFLAGS) -c -o $@ $<

# link to math libs for some avl tree calculations
test: CCFLAGS += -lm -g
test: $(OBJ_T)
	@for test in $(OBJ_T); do ./$$test; done

$(TDIR_O)/%.test: $(TDIR)/%.c
	@$(CC) $(CCFLAGS) -o $@ $<

debug: CCFLAGS += -g -DDEBUG -fsanitize=undefined,null,return,unreachable,shift,address
debug: rirc

clean:
	@echo cleaning
	@rm -f rirc $(SDIR_O)/*.o $(TDIR_O)/*.test

.PHONY: clean

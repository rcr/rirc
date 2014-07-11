CFLAGS = -std=c99 -Wall -pedantic -g

SDIR = src
TDIR = test

# Source and object files
SRC = $(wildcard $(SDIR)/*.c)
OBJ = $(patsubst $(SDIR)%.c,$(SDIR_O)%.o,$(SRC))
SDIR_O = $(SDIR)/bld

# Test source and executable files
SRC_T = $(wildcard $(TDIR)/*.c)
OBJ_T = $(patsubst $(TDIR)%.c,$(TDIR_O)%.test,$(SRC_T))
TDIR_O = $(TDIR)/bld

rirc: $(OBJ)
	cc $(CFLAGS) -o $@ $^

$(SDIR_O)/%.o: $(SDIR)/%.c
	cc $(CFLAGS) -c -o $@ $<

test: $(OBJ_T)
	@for test in $(OBJ_T); do ./$$test; done

$(TDIR_O)/%.test: $(TDIR)/%.c
	@cc $(CFLAGS) -o $@ $<

clean:
	@echo cleaning
	@rm -f rirc $(SDIR_O)/*.o $(TDIR_O)/*.test

.PHONY: clean

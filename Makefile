CC = cc
CFLAGS = -pthread -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -pthread

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

# link to math libs for some avl tree calculations
test: CFLAGS += -lm -g
test: $(OBJ_T)
	@for test in $(OBJ_T); do ./$$test; done

$(TDIR_O)/%.test: $(TDIR)/%.c
	@$(CC) $(CFLAGS) -o $@ $<

debug: CFLAGS += -g -DDEBUG
debug: rirc

clean:
	@echo cleaning
	@rm -f rirc $(SDIR_O)/*.o $(TDIR_O)/*.test

.PHONY: clean default

ODIR = bld
SDIR = src
CFLAGS = -std=c99 -Wall -pedantic -g

_OBJS = rirc.o input.o net.o ui.o

OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))

rirc : $(OBJS)
	cc $(CFLAGS) -o $@ $^

$(ODIR)/%.o: $(SDIR)/%.c $(SDIR)/common.h
	cc $(CFLAGS) -c -o $@ $<

.PHONY : clean
clean:
	-@rm -f rirc $(ODIR)/*.o

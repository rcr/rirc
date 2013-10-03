cflags = std=c99 -Wall -pedantic
objects = rirc.o net.o ui.o input.o

rirc : $(objects)
	cc $(cflags) -o rirc $(objects)

rirc.o :
	cc -c rirc.c

net.o :
	cc -c net.c

ui.o :
	cc -c ui.c

input.o :
	cc -c input.c

.PHONY :clean
clean :
	-@rm -rf rirc $(objects)

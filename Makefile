cflags = -std=c99 -Wall -pedantic
objects = rirc.o net.o ui.o input.o

rirc : $(objects)
	cc $(cflags) -o rirc $(objects)

rirc.o : rirc.c common.h
	cc -c rirc.c

net.o : net.c common.h
	cc -c net.c

ui.o : ui.c common.h
	cc -c ui.c

input.o : input.c common.h
	cc -c input.c

.PHONY : clean
clean :
	-@rm -rf rirc $(objects)

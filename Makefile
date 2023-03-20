VERSION = 0.1.6

CC      = cc
CFLAGS  = -flto -O2 -DNDEBUG
LDFLAGS = -flto

PREFIX   = /usr/local
PATH_BIN = $(DESTDIR)$(PREFIX)/bin
PATH_MAN = $(DESTDIR)$(PREFIX)/share/man/man1

SRC = \
	src/components/buffer.c \
	src/components/channel.c \
	src/components/input.c \
	src/components/ircv3.c \
	src/components/mode.c \
	src/components/server.c \
	src/components/user.c \
	src/draw.c \
	src/handlers/irc_ctcp.c \
	src/handlers/irc_recv.c \
	src/handlers/irc_send.c \
	src/handlers/ircv3.c \
	src/io.c \
	src/rirc.c \
	src/state.c \
	src/utils/utils.c \

OBJ = $(SRC:.c=.o)

all: rirc

include lib/mbedtls.Makefile

config.h:
	cp config.def.h config.h

rirc: config.h $(OBJ) $(MBEDTLS)
	$(CC) $(LDFLAGS) -pthread $(OBJ) $(MBEDTLS) -o $@

install: rirc
	@sed -i "s/VERSION/$(VERSION)/g" rirc.1
	mkdir -p $(PATH_BIN)
	mkdir -p $(PATH_MAN)
	cp -f rirc   $(PATH_BIN)
	cp -f rirc.1 $(PATH_MAN)
	chmod 755 $(PATH_BIN)/rirc
	chmod 644 $(PATH_MAN)/rirc.1

uninstall:
	rm -f $(PATH_BIN)/rirc
	rm -f $(PATH_MAN)/rirc.1

clean:
	@rm -f rirc $(MBEDTLS) $(OBJ)

%.o: %.c $(MBEDTLS)
	$(CC) -c $(CFLAGS) $(MBEDTLS_CFLAGS) -std=c11 -I. -D_POSIX_C_SOURCE=200809L -DVERSION=$(VERSION) $< -o $@

.PHONY: all clean options install uninstall

.SUFFIXES:

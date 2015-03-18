/* For addrinfo, getaddrinfo, getnameinfo */
#define _POSIX_C_SOURCE 200112L

#include <poll.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"

/* Connection thread info */
typedef struct connection_thread {
	int socket;
	int socket_tmp;
	char *host;
	char *port;
	char error[MAX_ERROR];
	char ipstr[INET6_ADDRSTRLEN];
	pthread_t tid;
} connection_thread;

static server* new_server(char*, char*);
static void _newline(channel*, line_t, const char*, const char*, size_t);

static int check_connect(server*);
static int check_latency(server*, time_t);
static int check_reconnect(server*, time_t);
static int check_socket(server*, time_t);

static int action_close_server(char);

static void server_connected(server*);

static void* threaded_connect(void*);
static void* threaded_connect_cleanup(void**);

/* Doubly linked list macros */
#define DLL_NEW(L, N) ((L) = (N)->next = (N)->prev = (N))

#define DLL_ADD(L, N) \
	do { \
		if ((L) == NULL) \
			DLL_NEW(L, N); \
		else { \
			((L)->next)->prev = (N); \
			(N)->next = ((L)->next); \
			(N)->prev = (L); \
			(L)->next = (N); \
		} \
	} while (0)

#define DLL_DEL(L, N) \
	do { \
		if (((N)->next) == (N)) \
			(L) = NULL; \
		else { \
			if ((L) == N) \
				(L) = ((N)->next); \
			((N)->next)->prev = ((N)->prev); \
			((N)->prev)->next = ((N)->next); \
		} \
	} while (0)

int
sendf(char *err, server *s, const char *fmt, ...)
{
	/* Send a formatted message to a server.
	 *
	 * Returns non-zero on failure and prints the error message to the buffer pointed
	 * to by err.
	 */

	char sendbuff[BUFFSIZE];
	int soc, len;
	va_list ap;

	if (s == NULL || (soc = s->soc) < 0) {
		strncpy(err, "Error: Not connected to server", MAX_ERROR);
		return 1;
	}

	va_start(ap, fmt);
	len = vsnprintf(sendbuff, BUFFSIZE-2, fmt, ap);
	va_end(ap);

	if (len < 0) {
		strncpy(err, "Error: Invalid message format", MAX_ERROR);
		return 1;
	}

	if (len >= BUFFSIZE-2) {
		strncpy(err, "Error: Message exceeds maximum length of " STR(BUFFSIZE) " bytes", MAX_ERROR);
		return 1;
	}

#ifdef DEBUG
	_newline(s->channel, 0, "DEBUG >>", sendbuff, len);
#endif

	sendbuff[len++] = '\r';
	sendbuff[len++] = '\n';

	if (send(soc, sendbuff, len, 0) < 0) {
		snprintf(err, MAX_ERROR, "Error: %s", strerror(errno));
		return 1;
	}

	return 0;
}

void
server_connect(char *host, char *port)
{
	connection_thread *ct;
	server *s;

	/* Check if a server matching host:port already exists */
	if ((s = server_head)) do {
		if (!strcmp(s->host, host) && !strcmp(s->port, port))
			break;
		s = s->next;
	} while (s != server_head || (s = NULL));

	if (s && s->soc >= 0) {
		newlinef((ccur = s->channel), 0, "-!!-", "Already connected to %s:%s", host, port);
		return;
	}

	ccur = s ? s->channel : (s = new_server(host, port))->channel;

	/* TODO: the calling function should take care of adding the server
	 * let server_connect return server* or NULL */
	DLL_ADD(server_head, s);

	if ((ct = calloc(1, sizeof(*ct))) == NULL)
		fatal("calloc");

	ct->socket = -1;
	ct->socket_tmp = -1;
	ct->host = s->host;
	ct->port = s->port;

	s->connecting = ct;

	newlinef(s->channel, 0, "--", "Connecting to '%s' port %s", host, port);

	if ((pthread_create(&ct->tid, NULL, threaded_connect, ct)))
		fatal("server_connect - pthread_create");
}

static void
server_connected(server *s)
{
	/* Server successfully connected, send IRC init messages */

	connection_thread *ct = s->connecting;

	if ((pthread_join(ct->tid, NULL)))
		fatal("server_connected - pthread_join");

	if (*ct->ipstr)
		newlinef(s->channel, 0, "--", "Connected to [%s]", ct->ipstr);
	else
		newlinef(s->channel, 0, "--", "Error determining server IP: %s", ct->error);

	s->soc = ct->socket;

	/* Set reconnect parameters to 0 in case this was an auto-reconnect */
	s->reconnect_time = 0;
	s->reconnect_delta = 0;

	s->latency_time = time(NULL);
	s->latency_delta = 0;

	sendf(NULL, s, "NICK %s", s->nick_me);
	sendf(NULL, s, "USER %s 8 * :%s", config.username, config.realname);
}

static void*
threaded_connect(void *arg)
{
	int ret;
	struct addrinfo hints, *p, *servinfo = NULL;

	/* Thread cleanup on error or external thread cancel */
	pthread_cleanup_push(threaded_connect_cleanup, &servinfo);

	memset(&hints, 0, sizeof(hints));

	/* IPv4 and/or IPv6 */
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	connection_thread *ct = (connection_thread *)arg;

	/* Resolve host */
	if ((ret = getaddrinfo(ct->host, ct->port, &hints, &servinfo))) {
		strncpy(ct->error, gai_strerror(ret), MAX_ERROR);
		pthread_exit(NULL);
	}

	/* Attempt to connect to all address results */
	for (p = servinfo; p != NULL; p = p->ai_next) {

		if ((ct->socket_tmp = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		/* Break on success */
		if (connect(ct->socket_tmp, p->ai_addr, p->ai_addrlen) == 0)
			break;

		close(ct->socket_tmp);
	}

	if (p == NULL) {
		strerror_r(errno, ct->error, MAX_ERROR);
		pthread_exit(NULL);
	}

	/* Failing to get the numeric IP isn't a fatal connection error */
	if ((ret = getnameinfo(p->ai_addr, p->ai_addrlen, ct->ipstr,
					INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST))) {
		strncpy(ct->error, gai_strerror(ret), MAX_ERROR);
	}

	/* Set non-blocking */
	if ((ret = fcntl(ct->socket_tmp, F_SETFL, O_NONBLOCK)) < 0) {
		strerror_r(errno, ct->error, MAX_ERROR);
		pthread_exit(NULL);
	}

	ct->socket = ct->socket_tmp;

	pthread_cleanup_pop(1);

	return NULL;
}

static void*
threaded_connect_cleanup(void **arg)
{
	struct addrinfo *servinfo = (struct addrinfo *)(*arg);

	if (servinfo)
		freeaddrinfo(servinfo);

	return NULL;
}

void
server_disconnect(server *s, char *err, char *mesg)
{
	/* When err is not null:
	 *   Disconnect initiated by remote host
	 *
	 * When mesg is not null:
	 *   Disconnect initiated by user */

	if (s->connecting) {
		/* Server connection in progress, cancel the connection attempt */

		connection_thread *ct = s->connecting;

		if ((pthread_cancel(ct->tid)))
			fatal("threaded_connect_cancel - pthread_cancel");

		/* There's a chance the thread is canceled with an open socket */
		if (ct->socket_tmp)
			close(ct->socket_tmp);

		free(ct);
		s->connecting = NULL;

		newlinef(s->channel, 0, "--", "Connection to '%s' port %s canceled", s->host, s->port);

		return;
	}

	if (s->soc >= 0) {

		if (err) {
			newlinef(s->channel, 0, "ERROR", "%s", err);
			newlinef(s->channel, 0, "--", "Attempting reconnect in %ds", RECONNECT_DELTA);

			/* If disconnecting due to error, attempt a reconnect */
			s->reconnect_time = time(NULL) + RECONNECT_DELTA;
			s->reconnect_delta = RECONNECT_DELTA;
		}

		if (mesg)
			sendf(NULL, s, "QUIT :%s", mesg);

		close(s->soc);

		/* Set all server attributes back to default */
		s->soc = -1;
		s->usermode = 0;
		s->iptr = s->input;
		s->nptr = config.nicks;
		s->latency_delta = 0;

		/* Reset the nick that reconnects will attempt to register with */
		auto_nick(&(s->nptr), s->nick_me);

		/* Print message to all open channels and reset their attributes */
		channel *c = s->channel;
		do {
			newline(c, 0, "-!!-", "(disconnected)");

			c->chanmode = 0;
			c->nick_count = 0;

			free_nicklist(c->nicklist);
			c->nicklist = NULL;

		} while ((c = c->next) != s->channel);

		return;
	}

	/* Cancel server reconnect attempts */
	if (s->reconnect_time) {
		newlinef(s->channel, 0, "--", "Auto reconnect attempt canceled");

		s->reconnect_time = 0;
		s->reconnect_delta = 0;

		return;
	}
}

/*
 * Server polling functions
 * */

void
check_servers(void)
{
	/* For each server, check the following, in order:
	 *
	 *  - Connection status. Skip the rest if unresolved
	 *  - Ping timeout.      Skip the rest detected
	 *  - Reconnect attempt. Skip the rest if successful
	 *  - Socket input.      Consume all input
	 *  */

	/* TODO: there's probably a better order to check these */

	server *s;

	if ((s = server_head) == NULL)
		return;

	time_t t = time(NULL);

	do {
		if (check_connect(s))
			continue;

		if (check_latency(s, t))
			continue;

		if (check_reconnect(s, t))
			continue;

		check_socket(s, t);

	} while ((s = s->next) != server_head);
}

static int
check_connect(server *s)
{
	/* Check the server's connection thread status for success or failure */

	if (!s->connecting)
		return 0;

	connection_thread *ct = (connection_thread*)s->connecting;

	/* Connection Success */
	if (ct->socket >= 0) {
		server_connected(s);

	/* Connection failure */
	} else if (*ct->error) {
		newline(s->channel, 0, "-!!-", ct->error);

		/* If server was auto-reconnecting, increase the backoff */
		if (s->reconnect_time) {
			s->reconnect_delta *= 2;
			s->reconnect_time += s->reconnect_delta;

			newlinef(s->channel, 0, "--", "Attempting reconnect in %ds", s->reconnect_delta);
		}

	/* Connection in progress */
	} else {
		return 1;
	}

	free(ct);
	s->connecting = NULL;

	return 1;
}

static int
check_latency(server *s, time_t t)
{
	/* Check time since last message */

	time_t delta;

	if (s->soc < 0)
		return 0;

	delta = t - s->latency_time;

	/* Timeout */
	if (delta > 255) {
		server_disconnect(s, "Ping timeout (255s)", NULL);
		return 1;
	}

	/* Display latency status for current server */
	if (delta > 120 && ccur->server == s) {
		s->latency_delta = delta;
		draw(D_STATUS);
	}

	return 0;
}

static int
check_reconnect(server *s, time_t t)
{
	/* Check if the server is in auto-reconnect mode, and issue a reconnect if needed */

	if (s->reconnect_time && t > s->reconnect_time) {
		server_connect(s->host, s->port);
		return 1;
	}

	return 0;
}

static int
check_socket(server *s, time_t t)
{
	/* Check the status of the server's socket */

	ssize_t count;
	char recv_buff[BUFFSIZE];

	/* Consume all input on the socket */
	while (s->soc >= 0 && (count = read(s->soc, recv_buff, BUFFSIZE)) >= 0) {

		if (count == 0) {
			server_disconnect(s, "Remote hangup", NULL);
			break;
		}

		/* Set time since last message */
		s->latency_time = t;
		s->latency_delta = 0;

		recv_mesg(recv_buff, count, s);
	}

	/* Server received ERROR message or remote hangup */
	if (s->soc < 0)
		return 0;

	/* Socket is non-blocking */
	if (errno != EWOULDBLOCK && errno != EAGAIN)
		fatal("read");

	return 0;
}

/* TODO:
 *
 * Everything below this comment has nothing to do with net and should be moved
 * to mesg.c or similar
 *
 * */

void
newline(channel *c, line_t type, const char *from, const char *mesg)
{
	/* Default wrapper for _newline because length of message won't be known */

	_newline(c, type, from, mesg, strlen(mesg));
}

void
newlinef(channel *c, line_t type, const char *from, const char *fmt, ...)
{
	/* Formating wrapper for _newline */

	char buff[BUFFSIZE];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buff, BUFFSIZE, fmt, ap);
	va_end(ap);

	_newline(c, type, from, buff, len);
}

static void
_newline(channel *c, line_t type, const char *from, const char *mesg, size_t len)
{
	/* Static function for handling inserting new lines into buffers */

	line *new_line;

	/* c->buffer_head points to the first printable line, so get the next line in the
	 * circular buffer */
	if ((new_line = c->buffer_head + 1) == &c->buffer[SCROLLBACK_BUFFER])
		new_line = c->buffer;

	/* Increment the channel's scrollback pointer if it pointed to the first or last line, ie:
	 *  - if it points to c->buffer_head, it pointed to the previous first line in the buffer
	 *  - if it points to new_line, it pointed to the previous last line in the buffer
	 *  */
	if (c->draw.scrollback == c->buffer_head || c->draw.scrollback == new_line)
		if (++c->draw.scrollback == &c->buffer[SCROLLBACK_BUFFER])
			c->draw.scrollback = c->buffer;

	c->buffer_head = new_line;

	/* new_channel() memsets c->buffer to 0, so this will either free(NULL) or an old line */
	free(new_line->text);

	if (c == NULL)
		fatal("channel is null");

	/* Set the line meta data */
	new_line->len = len;
	new_line->type = type;
	new_line->time = time(NULL);

	/* Rows are recalculated by the draw routine when == 0 */
	new_line->rows = 0;

	/* If from is NULL, assume server message */
	strncpy(new_line->from, (from) ? from : c->name, NICKSIZE);

	size_t len_from;
	if ((len_from = strlen(new_line->from)) > c->draw.nick_pad)
		c->draw.nick_pad = len_from;

	if (mesg == NULL)
		fatal("mesg is null");

	if ((new_line->text = malloc(new_line->len + 1)) == NULL)
		fatal("newline");

	strcpy(new_line->text, mesg);

	if (c == ccur)
		draw(D_BUFFER);
	else if (!type && c->active < ACTIVITY_ACTIVE) {
		c->active = ACTIVITY_ACTIVE;
		draw(D_CHANS);
	}
}

static server*
new_server(char *host, char *port)
{
	server *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc");

	/* Set non-zero default fields */
	s->soc = -1;
	s->iptr = s->input;
	s->nptr = config.nicks;
	s->host = strdup(host);
	s->port = strdup(port);

	auto_nick(&(s->nptr), s->nick_me);

	s->channel = ccur = new_channel(host, s, NULL);

	return s;
}

channel*
new_channel(char *name, server *server, channel *chanlist)
{
	channel *c;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc");

	c->server = server;
	c->buffer_head = c->buffer;
	c->active = ACTIVITY_DEFAULT;
	c->input = new_input();
	c->draw.scrollback = c->buffer_head;

	/* TODO: if channel name length exceeds CHANSIZE we'll never appropriately
	 * associate incomming messages with this channel anyways so it shouldn't be allowed */
	strncpy(c->name, name, CHANSIZE);

	/* Append the new channel to the list */
	DLL_ADD(chanlist, c);

	draw(D_FULL);

	return c;
}

void
free_server(server *s)
{
	/* TODO: s->connecting???  should free ct, pthread cancel, close the socket, etc */
	/* TODO: close the socket? send_quit is expected me to? */

	channel *t, *c = s->channel;
	do {
		t = c;
		c = c->next;
		free_channel(t);
	} while (c != s->channel);

	free(s->host);
	free(s->port);
	free(s);
}

void
free_channel(channel *c)
{
	line *l;
	for (l = c->buffer; l < c->buffer + SCROLLBACK_BUFFER; l++)
		free(l->text);

	free_nicklist(c->nicklist);
	free_input(c->input);
	free(c);
}

channel*
channel_get(char *chan, server *s)
{
	channel *c = s->channel;

	do {
		if (!strcmp(c->name, chan))
			return c;
		c = c->next;
	} while (c != s->channel);

	return NULL;
}

void
clear_channel(channel *c)
{
	free(c->buffer_head->text);

	c->buffer_head->text = NULL;

	c->draw.nick_pad = 0;

	draw(D_BUFFER);
}

/* Confirm closing a server */
static int
action_close_server(char c)
{
	if (c == 'n' || c == 'N')
		return 1;

	if (c == 'y' || c == 'Y') {

		channel *c = ccur;

		/* If closing the last server */
		if ((ccur = c->server->next->channel) == c->server->channel)
			ccur = rirc;

		server_disconnect(c->server, NULL, DEFAULT_QUIT_MESG);

		DLL_DEL(server_head, c->server);
		free_server(c->server);

		draw(D_FULL);

		return 1;
	}

	return 0;
}

/* Close a channel buffer/server and return the next channel */
channel*
channel_close(channel *c)
{
	/* Close a buffer,
	 *
	 * if closing a server buffer, confirm with the user */

	channel *ret = c;

	/* c in this case is the main buffer */
	if (c->server == NULL)
		return c;

	if (!c->type) {
		/* Closing a server, confirm the number of channels being closed */

		int num_chans = 0;

		while ((c = c->next)->type)
			num_chans++;

		if (num_chans)
			action(action_close_server, "Close server '%s'? Channels: %d   [y/n]",
					c->server->host, num_chans);
		else
			action(action_close_server, "Close server '%s'?   [y/n]", c->server->host);
	} else {
		/* Closing a channel */

		sendf(NULL, c->server, "PART %s", c->name);

		/* If channel c is last in the list, return the previous channel */
		ret = !(c->next == c->server->channel) ?
			c->next : c->prev;

		DLL_DEL(c->server->channel, c);
		free_channel(c);

		draw(D_FULL);
	}

	return ret;
}

/* Get a channel's next/previous, taking into account server wraparound */
channel*
channel_switch(channel *c, int next)
{
	channel *ret;

	/* c in this case is the main buffer */
	if (c->server == NULL)
		return c;

	if (next)
		/* If wrapping around forwards, get next server's first channel */
		ret = !(c->next == c->server->channel) ?
			c->next : c->server->next->channel;
	else
		/* If wrapping around backwards, get previous server's last channel */
		ret = !(c == c->server->channel) ?
			c->prev : c->server->prev->channel->prev;

	ret->active = ACTIVITY_DEFAULT;

	draw(D_FULL);

	return ret;
}

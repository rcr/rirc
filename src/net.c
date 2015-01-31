/* For addrinfo, getaddrinfo, getnameinfo */
#define _POSIX_C_SOURCE 200112L

#include <time.h>
#include <poll.h>
#include <stdio.h>
#include <ctype.h>
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

/* Numeric Reply Codes */
#define RPL_WELCOME            1
#define RPL_YOURHOST           2
#define RPL_CREATED            3
#define RPL_MYINFO             4
#define RPL_ISUPPORT           5
#define RPL_STATSCONN        250
#define RPL_LUSERCLIENT      251
#define RPL_LUSEROP          252
#define RPL_LUSERUNKNOWN     253
#define RPL_LUSERCHANNELS    254
#define RPL_LUSERME          255
#define RPL_LOCALUSERS       265
#define RPL_GLOBALUSERS      266
#define RPL_CHANNEL_URL      328
#define RPL_NOTOPIC          331
#define RPL_TOPIC            332
#define RPL_TOPICWHOTIME     333
#define RPL_NAMREPLY         353
#define RPL_ENDOFNAMES       366
#define RPL_MOTD             372
#define RPL_MOTDSTART        375
#define RPL_ENDOFMOTD        376
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE    433

/* TODO: 351, reply from server for /version
 *       401, no such nick or channel (eg /version <unknown nick>)
 * TODO: 409, :No origin specified, (eg "/raw PING")
 * TODO: 402, no such server
 * TODO: 435, cannot change nickname while banned
 *            (eg unregistered in channel and trying to /nick)
 * TODO: 403, :no such channel
 * TODO: 451, :Connection not registered
 *            (eg sending a join before registering) */

/* Error message length */
#define MAX_ERROR 512

#define DEFAULT_QUIT_MESG "rirc v" VERSION " -- http://rcr.io/rirc.html"

#define IS_ME(X) !strcmp(X, s->nick_me)

/* Advance a message pointer until non-blank character */
#define skip_space(X) while (*X && isblank(*X)) X++;

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

/* Message receiving handlers */
char* recv_ctcp_req(parsed_mesg*, server*);
char* recv_ctcp_rpl(parsed_mesg*);
char* recv_error(parsed_mesg*, server*);
char* recv_join(parsed_mesg*, server*);
char* recv_mode(parsed_mesg*, server*);
char* recv_nick(parsed_mesg*, server*);
char* recv_notice(parsed_mesg*, server*);
char* recv_numeric(parsed_mesg*, server*);
char* recv_part(parsed_mesg*, server*);
char* recv_ping(parsed_mesg*, server*);
char* recv_priv(parsed_mesg*, server*);
char* recv_quit(parsed_mesg*, server*);

/* Message sending handlers */
static char* send_connect(char*);
static char* send_default(char*);
static char* send_disconnect(char*);
static char* send_emote(char*);
static char* send_join(char*);
static char* send_nick(char*);
static char* send_part(char*);
static char* send_priv(char*);
static char* send_quit(char*);
static char* send_raw(char*);
static char* send_version(char*);

channel* channel_get(char*, server*);
server* new_server(char*, char*);
void free_channel(channel*);
void free_server(server*);
void get_auto_nick(char**, char*);
void recv_mesg(char*, int, server*);

static int confirm_server_close(char);

static char* sendf(server *restrict, const char*, ...);

static void server_connected(server*);
static void server_disconnect(server*, char*, char*);

static void* threaded_connect(void*);
static void* threaded_connect_cleanup(void**);

static server *server_head;

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
		newlinef((ccur = s->channel), 0, "-!!-","Already connected to %s:%s", host, port);
		draw(D_STATUS);
		return;
	}

	ccur = s ? s->channel : (s = new_server(host, port))->channel;

	DLL_ADD(server_head, s);

	if ((ct = malloc(sizeof(connection_thread))) == NULL)
		fatal("server_connect - malloc");

	ct->socket = -1;
	ct->socket_tmp = -1;
	ct->host = s->host;
	ct->port = s->port;
	*ct->error = '\0';
	*ct->ipstr = '\0';

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

	sendf(s, "NICK %s", s->nick_me);
	sendf(s, "USER %s 8 * :%s", config.username, config.realname);
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
		*ct->ipstr = '\0';
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
check_servers(void)
{
	int ret, count;

	static char recv_buff[BUFFSIZE];

	static struct pollfd pfd[] = {{ .events = POLLIN }};

	server *s;

	if ((s = server_head) == NULL)
		return;

	do {
		pfd[0].fd = s->soc;

		if (s->connecting) {

			connection_thread *ct = (connection_thread*)s->connecting;

			/* Connection Success */
			if (ct->socket >= 0)
				server_connected(s);

			/* Connection failure */
			else if (*ct->error)
				newline(s->channel, 0, "-!!-", ct->error, 0);

			/* Connection in progress */
			else
				continue;

			free(ct);
			s->connecting = NULL;

		} else while (s->soc > 0 && (ret = poll(pfd, 1, 0)) > 0) {
			if ((count = read(s->soc, recv_buff, BUFFSIZE)) < 0)
				; /* TODO: error in read() */
			else if (count == 0)
				server_disconnect(s, "Remote hangup", NULL);
			else
				recv_mesg(recv_buff, count, s);
		}

		if (ret < 0) {
			; /* TODO: error in poll() */
		}

		s = s->next;
	} while (s != server_head);
}

static void
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

		if (err)
			newlinef(s->channel, 0, "ERROR", "%s", err);

		if (mesg)
			sendf(s, "QUIT :%s", mesg);

		close(s->soc);

		/* Set all server attributes back to default */
		s->usermode = 0;
		s->soc = -1;

		channel *c = s->channel;
		do {
			newline(c, 0, "-!!-", "(disconnected)", 0);

			c->chanmode = 0;
			c->nick_count = 0;

			free_nicklist(c->nicklist);
			c->nicklist = NULL;

			c = c->next;
		} while (c != s->channel);
	}
}

/* FIXME: some clients can actually send a null message.
 * this messes up the printing process by thinking there
 * are no more lines to print and effectively 'clears' the screen */
/* XXX: the routine that prints should check if the POINTER is null,
 * not the contents of the pointer, thats how we know for sure that
 * its an empty message */
void
newline(channel *c, line_t type, const char *from, const char *mesg, int len)
{
	time_t raw_t;
	struct tm *t;

	if (len == 0)
		len = strlen(mesg);

	if (++c->buffer_head == &c->buffer[SCROLLBACK_BUFFER])
		c->buffer_head = c->buffer;

	line *l = c->buffer_head;

	free(l->text);

	l->len = len;
	if ((l->text = malloc(len + 1)) == NULL)
		fatal("newline");
	strcpy(l->text, mesg);

	time(&raw_t);
	t = localtime(&raw_t);
	l->time_h = t->tm_hour;
	l->time_m = t->tm_min;

	/* Since most lines are added to buffers not beind drawn, set zero */
	l->rows = 0;

	l->type = type;

	if (!from) /* Server message */
		strncpy(l->from, c->name, 50);
	else
		strncpy(l->from, from, 50);

	int len_from;
	if ((len_from = strlen(l->from)) > c->nick_pad)
		c->nick_pad = len_from;

	if (c == ccur)
		draw(D_BUFFER);
	else if (!type && c->active < ACTIVITY_ACTIVE)
		c->active = ACTIVITY_ACTIVE;

	draw(D_CHANS);
}

void
newlinef(channel *c, line_t type, const char *from, const char *fmt, ...)
{
	char buff[BUFFSIZE];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buff, BUFFSIZE, fmt, ap);
	va_end(ap);

	newline(c, type, from, buff, len);
}

void
get_auto_nick(char **autonick, char *nick)
{
	char *p = *autonick;
	while (*p == ' ' || *p == ',')
		p++;

	if (*p == '\0') {
		/* Autonicks exhausted, generate a random nick */
		char *base = "rirc_";
		char *cset = "0123456789ABCDEF";

		strcpy(nick, base);
		nick += strlen(base);

		int i, len = strlen(cset);
		for (i = 0; i < 4; i++)
			*nick++ = cset[rand() % len];
	} else {
		int c = 0;
		while (*p != ' ' && *p != ',' && *p != '\0' && c++ < 50)
			*nick++ = *p++;
		*autonick = p;
	}

	*nick = '\0';
}

server*
new_server(char *host, char *port)
{
	server *s;
	if ((s = malloc(sizeof(server))) == NULL)
		fatal("new_server");

	s->soc = -1;
	s->usermode = 0;
	s->iptr = s->input;
	s->nptr = config.nicks;
	s->host = strdup(host);
	s->port = strdup(port);
	s->connecting = NULL;

	get_auto_nick(&(s->nptr), s->nick_me);

	s->channel = ccur = new_channel(host, s, NULL);

	return s;
}

channel*
new_channel(char *name, server *server, channel *chanlist)
{
	channel *c;
	if ((c = malloc(sizeof(channel))) == NULL)
		fatal("new_channel");

	c->type = '\0';
	c->parted = 0;
	c->resized = 0;
	c->nick_pad = 0;
	c->chanmode = 0;
	c->nick_count = 0;
	c->nicklist = NULL;
	c->server = server;
	c->buffer_head = c->buffer;
	c->active = ACTIVITY_DEFAULT;
	c->input = new_input();
	strncpy(c->name, name, 50);
	memset(c->buffer, 0, sizeof(c->buffer));

	DLL_ADD(chanlist, c);

	draw(D_FULL);

	return c;
}

void
free_server(server *s)
{
	/* TODO: s->connecting??? */
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

/* Confirm closing a server */
static int
confirm_server_close(char c)
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
			confirm(confirm_server_close, "Close server '%s'? Channels: %d   [y/n]",
					c->server->host, num_chans);
		else
			confirm(confirm_server_close, "Close server '%s'?   [y/n]", c->server->host);
	} else {
		/* Closing a channel */

		sendf(c->server, "PART %s", c->name);

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

static char*
sendf(server *restrict s, const char *fmt, ...)
{
	/* Send a formatted message to a server.
	 *
	 * Safe to call without formatting as long as '%' is escaped (%%), however
	 * it's not recommended to do so with anything besides string literals.
	 *
	 * e.g.:
	 * sendf(server, "hello world");  ...Okay.
	 * sendf(server, char_pointer);   ...Not okay.
	 */

	char sendbuff[BUFFSIZE];
	int soc, len;
	va_list ap;

	if (s == NULL || (soc = s->soc) < 0)
		return "Error: Not connected to server";

	va_start(ap, fmt);
	len = vsnprintf(sendbuff, BUFFSIZE-2, fmt, ap);
	va_end(ap);

	if (len < 0)
		return "Error: Invalid message format";

	if (len >= BUFFSIZE-2)
		return "Error: Message exceeds maximum length of " STR(BUFFSIZE) " bytes";

#ifdef DEBUG
	newline(s->channel, LINE_DEBUG, "DEBUG >>", sendbuff, len);
#endif

	sendbuff[len++] = '\r';
	sendbuff[len++] = '\n';

	if (send(soc, sendbuff, len, 0) < 0)
		return errf("Error: %s", strerror(errno));

	return NULL;
}

/*
 * Message sending handlers
 * */

void
send_mesg(char *mesg)
{
	char *cmd, *err = NULL;

	if (*mesg != '/')
		err = send_default(mesg);
	else {
		mesg++;

		if (!(cmd = getarg(&mesg, 1)))
			err = "Messages beginning with '/' require a command";
		else if (!strcasecmp(cmd, "JOIN"))
			err = send_join(mesg);
		else if (!strcasecmp(cmd, "CONNECT"))
			err = send_connect(mesg);
		else if (!strcasecmp(cmd, "DISCONNECT"))
			err = send_disconnect(mesg);
		else if (!strcasecmp(cmd, "CLOSE"))
			ccur = channel_close(ccur);
		else if (!strcasecmp(cmd, "PART"))
			err = send_part(mesg);
		else if (!strcasecmp(cmd, "NICK"))
			err = send_nick(mesg);
		else if (!strcasecmp(cmd, "QUIT"))
			err = send_quit(mesg);
		else if (!strcasecmp(cmd, "MSG"))
			err = send_priv(mesg);
		else if (!strcasecmp(cmd, "PRIV"))
			err = send_priv(mesg);
		else if (!strcasecmp(cmd, "ME"))
			err = send_emote(mesg);
		else if (!strcasecmp(cmd, "VERSION"))
			err = send_version(mesg);
		else if (!strcasecmp(cmd, "RAW"))
			err = send_raw(mesg);
		else
			err = errf("Unknown command: %.*s%s",
				15, cmd, strlen(cmd) > 15 ? "..." : "");
	}

	if (err)
		newline(ccur, 0, "-!!-", err, 0);
}

static char*
send_connect(char *mesg)
{
	/* /connect [(host) | (host:port) | (host port)] */

	char *host, *port;

	if (!(host = strtok(mesg, " :"))) {

		if (!ccur->server || ccur->server->soc >= 0 || ccur->server->connecting)
			return "Error: Connect requires a hostname argument";

		/* If no hostname arg and server is disconnected, attempt to reconnect */
		host = ccur->server->host;
		port = ccur->server->port;

	} else if (!(port = strtok(NULL, " ")))
		port = "6667";

	server_connect(host, port);

	return NULL;
}

static char*
send_default(char *mesg)
{
	/* All messages not beginning with '/'  */

	if (!ccur->type)
		return "Error: This is not a channel";

	if (ccur->parted)
		return "Error: Parted from channel";

	char *err;
	if ((err = sendf(ccur->server, "PRIVMSG %s :%s", ccur->name, mesg)))
		return err;

	newline(ccur, 0, ccur->server->nick_me, mesg, 0);

	return NULL;
}

static char*
send_disconnect(char *mesg)
{
	/* /disconnect [quit message] */

	if (!ccur->server || (!ccur->server->connecting && ccur->server->soc < 0))
		return "Error: Not connected to server";

	skip_space(mesg);

	server_disconnect(ccur->server, NULL, (*mesg) ? mesg : DEFAULT_QUIT_MESG);

	return NULL;
}

static char*
send_emote(char *mesg)
{
	/* /me <message> */

	if (!ccur->type)
		return "Error: This is not a channel";

	if (ccur->parted)
		return "Error: Parted from channel";

	char *err;
	if ((err = sendf(ccur->server, "PRIVMSG %s :\x01""ACTION %s\x01", ccur->name, mesg)))
		return err;

	newlinef(ccur, LINE_ACTION, "*", "%s %s", ccur->server->nick_me, mesg);

	return NULL;
}

static char*
send_join(char *mesg)
{
	/* /join [target[,targets]*] */

	char *targ;

	if ((targ = strtok(mesg, " ")))
		return sendf(ccur->server, "JOIN %s", targ);

	if (!ccur->parted)
		return "Error: Not parted from channel";

	if (!ccur->type || ccur->type == 'p')
		return "Error: Can't rejoin server/private buffers";

	return sendf(ccur->server, "JOIN %s", ccur->name);
}

static char*
send_nick(char *mesg)
{
	/* /nick [nick] */

	char *nick;

	if ((nick = strtok(mesg, " ")))
		return sendf(ccur->server, "NICK %s", mesg);

	newlinef(ccur, 0, "--", "Your nick is %s", ccur->server->nick_me);

	return NULL;
}

static char*
send_part(char *mesg)
{
	/* /part [target[,targets]*] [part message]*/

	char *targ;

	if ((targ = strtok_r(mesg, " ", &mesg)))
		return sendf(ccur->server, "PART %s :%s", targ, (*mesg) ? mesg : DEFAULT_QUIT_MESG);

	if (ccur->parted)
		return "Error: Already parted from channel";

	if (!ccur->type || ccur->type == 'p')
		return "Error: Can't part server/private buffers";

	return sendf(ccur->server, "PART %s :%s", ccur->name, DEFAULT_QUIT_MESG);
}

static char*
send_priv(char *mesg) {
	/* /(priv | msg) <target> <message> */

	char *targ;

	if (!(targ = strtok_r(mesg, " ", &mesg)))
		return "Error: Private messages require a target";

	if (*mesg == '\0')
		return "Error: Private messages was null";

	return sendf(ccur->server, "PRIVMSG %s :%s", targ, mesg);
	/* TODO: if success, print to current channel?
	 * or find/open new channel? */
}

static char*
send_raw(char *mesg)
{
	/* /raw <raw message> */

	char *err;
	if ((err = sendf(ccur->server, "%s", mesg)))
		return err;

	newline(ccur, 0, "RAW >>", mesg, 0);

	return NULL;
}

static char*
send_quit(char *mesg)
{
	/* /quit [quit message] */

	skip_space(mesg);

	server *s, *t;
	if ((s = server_head)) do {

		if (s->soc >= 0) {
			sendf(ccur->server, "QUIT :%s", (*mesg) ? mesg : DEFAULT_QUIT_MESG);
			close(s->soc);
		}

		t = s;
		s = s->next;
		free_server(t);
	} while (s != server_head);

	free_channel(rirc);

#ifndef DEBUG
	/* Clear screen */
	printf("\x1b[H\x1b[J");
#endif

	exit(EXIT_SUCCESS);

	return NULL;
}

static char*
send_version(char *mesg)
{
	/* /version [target] */

	char *targ;

	if (ccur == rirc) {
		newline(ccur, 0, "--", "rirc version " VERSION, 0);
		newline(ccur, 0, "--", "http://rcr.io/rirc.html", 0);
		return NULL;
	}

	if ((targ = strtok(mesg, " "))) {
		newlinef(ccur, 0, "--", "Sending CTCP VERSION request to %s", targ);
		return sendf(ccur->server, "PRIVMSG %s :\x01""VERSION\x01", targ);
	} else {
		newlinef(ccur, 0, "--", "Sending CTCP VERSION request to %s", ccur->server);
		return sendf(ccur->server, "VERSION");
	}
}

/*
 * Message receiving handlers
 * */

void
recv_mesg(char *inp, int count, server *s)
{
	char *ptr = s->iptr;
	char *max = s->input + BUFFSIZE;

	static parsed_mesg *p;

	while (count--) {
		if (*inp == '\r') {

			*ptr = '\0';

#ifdef DEBUG
			newline(s->channel, LINE_DEBUG, "DEBUG <<", s->input, 0);
#endif
			char *err = NULL;

			if (!(p = parse(s->input)))
				err = "Failed to parse message";
			else if (isdigit(*p->command))
				err = recv_numeric(p, s);
			else if (!strcmp(p->command, "PRIVMSG"))
				err = recv_priv(p, s);
			else if (!strcmp(p->command, "JOIN"))
				err = recv_join(p, s);
			else if (!strcmp(p->command, "PART"))
				err = recv_part(p, s);
			else if (!strcmp(p->command, "QUIT"))
				err = recv_quit(p, s);
			else if (!strcmp(p->command, "NOTICE"))
				err = recv_notice(p, s);
			else if (!strcmp(p->command, "NICK"))
				err = recv_nick(p, s);
			else if (!strcmp(p->command, "PING"))
				err = recv_ping(p, s);
			else if (!strcmp(p->command, "MODE"))
				err = recv_mode(p, s);
			else if (!strcmp(p->command, "ERROR"))
				err = recv_error(p, s);
			else
				err = errf("Message type '%s' unknown", p->command);

			if (err)
				newline(s->channel, 0, "-!!-", err, 0);

			ptr = s->input;

		/* Don't accept unprintable characters unless space or ctcp markup */
		} else if (ptr < max && (isgraph(*inp) || *inp == ' ' || *inp == 0x01))
			*ptr++ = *inp;

		inp++;
	}

	s->iptr = ptr;
}

char*
recv_ctcp_req(parsed_mesg *p, server *s)
{
	/* CTCP Requests:
	 * PRIVMSG <target> :0x01<command> <arguments>0x01 */

	char *targ, *cmd, *ptr;
	channel *c;

	if (!p->from)
		return "CTCP: sender's nick is null";

	if (!(targ = getarg(&p->params, 1)))
		return "CTCP: target is null";

	/* Validate markup */
	ptr = ++p->trailing;

	while (*ptr && *ptr != 0x01)
		ptr++;

	if (*ptr == 0x01)
		*ptr = '\0';
	else
		return "CTCP: Invalid markup";

	/* Markup is valid, get command */
	if (!(cmd = getarg(&p->trailing, 1)))
		return "CTCP: command is null";

	if (!strcmp(cmd, "ACTION")) {

		/* FIXME: wrong??? shouldnt this be p->from?
		 * it should be from the message target... */
		/* FIXME: right now this opens a new private chat channel */
		if ((c = channel_get(p->from, s)) == NULL) {
			c = new_channel(p->from, s, s->channel);
			c->type = 'p';
		}

		newlinef(c, LINE_ACTION, "*", "%s %s", p->from, p->trailing);
		return NULL;
	}

	if (!strcmp(cmd, "VERSION")) {

		if ((c = channel_get(p->from, s)) == NULL)
			c = s->channel;

		newlinef(c, 0, "--", "Received CTCP VERSION from %s", p->from);
		sendf(s, "NOTICE %s :\x01""VERSION rirc version "VERSION"\x01", p->from);
		sendf(s, "NOTICE %s :\x01""VERSION http://rcr.io/rirc.html\x01", p->from);
		return NULL;
	}

	sendf(s, "NOTICE %s :\x01""ERRMSG %s\x01", p->from, cmd);
	return errf("CTCP: unknown command '%s'", cmd);
}

char*
recv_ctcp_rpl(parsed_mesg *p __attribute__((unused)))
{
	/* CTCP replies:
	 * NOTICE <target> :0x01<command> <arguments>0x01 */

	/* TODO */
	return NULL;
}

char*
recv_error(parsed_mesg *p, server *s)
{
	/* ERROR :<message> */

	server_disconnect(s, p->trailing ? p->trailing : "Remote hangup", NULL);

	return NULL;
}

char*
recv_join(parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain JOIN [:]<channel> */

	char *chan;
	channel *c;

	if (!p->from)
		return "JOIN: sender's nick is null";

	/* Check params first, then trailing */
	if (!(chan = getarg(&p->params, 1)) && !(chan = getarg(&p->trailing, 1)))
		return "JOIN: channel is null";

	if (IS_ME(p->from)) {
		if ((c = channel_get(chan, s)) == NULL)
			ccur = new_channel(chan, s, ccur);
		else {
			c->parted = 0;
			newlinef(c, LINE_JOIN, ">", "You have rejoined %s", chan);
		}
		draw(D_FULL);
	} else {

		if ((c = channel_get(chan, s)) == NULL)
			return errf("JOIN: channel '%s' not found", chan);

		if (nicklist_insert(&(c->nicklist), p->from)) {
			c->nick_count++;

			if (c->nick_count < config.join_part_quit_threshold)
				newlinef(c, LINE_JOIN, ">", "%s has joined %s", p->from, chan);

			draw(D_STATUS);
		} else {
			return errf("JOIN: nick '%s' already in '%s'", p->from, chan);
		}
	}

	return NULL;
}

char*
recv_mode(parsed_mesg *p, server *s)
{
	/* :nick MODE <targ> :<flags> */

	char *targ, *flags;

	if (!(targ = getarg(&p->params, 1)))
		return "MODE: target is null";

	if (!(flags = p->trailing))
		return "MODE: flags are null";

	int modebit;
	char plusminus = '\0';

	channel *c;
	if ((c = channel_get(targ, s))) {

		newlinef(c, 0, "--", "%s chanmode: [%s]", targ, flags);

		int *chanmode = &c->chanmode;

		/* Chanmodes */
		do {
			if (*flags == '+' || *flags == '-')
				plusminus = *flags;
			else if (plusminus == '\0') {
				return "MODE: +/- flag is null";
			} else {
				switch (*flags) {
					case 'O':
						modebit = CMODE_O;
						break;
					case 'o':
						modebit = CMODE_o;
						break;
					case 'v':
						modebit = CMODE_v;
						break;
					case 'a':
						modebit = CMODE_a;
						break;
					case 'i':
						modebit = CMODE_i;
						break;
					case 'm':
						modebit = CMODE_m;
						break;
					case 'n':
						modebit = CMODE_n;
						break;
					case 'q':
						modebit = CMODE_q;
						break;
					case 'p':
						modebit = CMODE_p;
						break;
					case 's':
						modebit = CMODE_s;
						break;
					case 'r':
						modebit = CMODE_r;
						break;
					case 't':
						modebit = CMODE_t;
						break;
					case 'k':
						modebit = CMODE_k;
						break;
					case 'l':
						modebit = CMODE_l;
						break;
					case 'b':
						modebit = CMODE_b;
						break;
					case 'e':
						modebit = CMODE_e;
						break;
					case 'I':
						modebit = CMODE_I;
						break;
					default:
						modebit = 0;
						newlinef(s->channel, 0, "-!!-", "Unknown mode '%c'", *flags);
				}
				if (modebit) {
					if (plusminus == '+')
						*chanmode |= modebit;
					else
						*chanmode &= ~modebit;
				}
			}
		} while (*(++flags) != '\0');
	}

	if (IS_ME(targ)) {

		newlinef(s->channel, 0, "--", "%s usermode: [%s]", targ, flags);

		int *usermode = &s->usermode;

		/* Usermodes */
		do {
			if (*flags == '+' || *flags == '-')
				plusminus = *flags;
			else if (plusminus == '\0') {
				return "MODE: +/- flag is null";
			} else {
				switch (*flags) {
					case 'a':
						modebit = UMODE_a;
						break;
					case 'i':
						modebit = UMODE_i;
						break;
					case 'w':
						modebit = UMODE_w;
						break;
					case 'r':
						modebit = UMODE_r;
						break;
					case 'o':
						modebit = UMODE_o;
						break;
					case 'O':
						modebit = UMODE_O;
						break;
					case 's':
						modebit = UMODE_s;
						break;
					default:
						modebit = 0;
						newlinef(s->channel, 0, "-!!-", "Unknown mode '%c'", *flags);
				}
				if (modebit) {
					if (plusminus == '+')
						*usermode |= modebit;
					else
						*usermode &= ~modebit;
				}
			}
		} while (*(++flags) != '\0');

		draw(D_STATUS);
	}

	return NULL;
}

char*
recv_nick(parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain NICK [:]<new nick> */

	char *nick;

	if (!p->from)
		return "NICK: old nick is null";

	/* Some servers seem to send the new nick in the trailing */
	if (!(nick = getarg(&p->params, 1)) && !(nick = getarg(&p->trailing, 1)))
		return "NICK: new nick is null";

	if (IS_ME(p->from)) {
		strncpy(s->nick_me, nick, NICKSIZE-1);
		newlinef(s->channel, 0, "--", "You are now known as %s", nick);
	}

	channel *c = s->channel;
	do {
		if (nicklist_delete(&c->nicklist, p->from)) {
			nicklist_insert(&c->nicklist, nick);
			newlinef(c, LINE_NICK, "--", "%s  >>  %s", p->from, nick);
		}
		c = c->next;
	} while (c != s->channel);

	return NULL;
}

char*
recv_notice(parsed_mesg *p, server *s)
{
	/* :nick.hostname.domain NOTICE <target> :<message> */

	char *targ;
	channel *c;

	if (!p->trailing)
		return "NOTICE: message is null";

	/* CTCP reply */
	if (*p->trailing == 0x01)
		return recv_ctcp_rpl(p);

	if (!(targ = getarg(&p->params, 1)))
		return "NOTICE: target is null";

	if ((c = channel_get(targ, s)))
		newline(c, 0, 0, p->trailing, 0);
	else
		newline(s->channel, 0, 0, p->trailing, 0);

	return NULL;
}

char*
recv_numeric(parsed_mesg *p, server *s)
{
	/* Numeric types: https://www.alien.net.au/irc/irc2numerics.html */

	channel *c;
	char *chan, *time, *type, *num;

	/* First parameter in numerics is always your nick */
	char *nick = getarg(&p->params, 1);

	/* Extract numeric code */
	int code = 0;
	do {
		code = code * 10 + (*p->command++ - '0');

		if (code > 999)
			return "NUMERIC: greater than 999";

	} while (isdigit(*p->command));

	/* Shortcuts */
	if (!code)
		return "NUMERIC: code is null";
	else if (code > 400) goto num_400;
	else if (code > 200) goto num_200;

	/* Numeric types (000, 200) */
	switch (code) {

	/* 001 <nick> :<Welcome message> */
	case RPL_WELCOME:

		/* Reset list of auto nicks */
		s->nptr = config.nicks;

		if (config.auto_join) {
			/* Only send the autojoin on command-line connect */
			sendf(s, "JOIN %s", config.auto_join);
			config.auto_join = NULL;
		} else {
			/* If reconnecting to server, join any non-parted channels */
			c = s->channel;
			do {
				if (c->type && c->type != 'p' && !c->parted)
					sendf(s, "JOIN %s", c->name);
				c = c->next;
			} while (c != s->channel);
		}

		newline(s->channel, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_YOURHOST:  /* 002 <nick> :<Host info, server version, etc> */
	case RPL_CREATED:   /* 003 <nick> :<Server creation date message> */

		newline(s->channel, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_MYINFO:    /* 004 <nick> <params> :Are supported by this server */
	case RPL_ISUPPORT:  /* 005 <nick> <params> :Are supported by this server */

		newlinef(s->channel, LINE_NUMRPL, "--", "%s ~ %s", p->params, p->trailing);
		return NULL;


	default:

		newlinef(s->channel, LINE_NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

num_200:

	/* Numeric types (200, 400) */
	switch (code) {

	/* 328 <channel> :<url> */
	case RPL_CHANNEL_URL:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_CHANNEL_URL: channel is null";

		if ((c = channel_get(chan, s)) == NULL)
			return errf("RPL_CHANNEL_URL: channel '%s' not found", chan);

		newlinef(c, LINE_NUMRPL, "--", "URL for %s is: \"%s\"", chan, p->trailing);
		return NULL;


	/* 332 <channel> :<topic> */
	case RPL_TOPIC:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_TOPIC: channel is null";

		if ((c = channel_get(chan, s)) == NULL)
			return errf("RPL_TOPIC: channel '%s' not found", chan);

		newlinef(c, LINE_NUMRPL, "--", "Topic for %s is \"%s\"", chan, p->trailing);
		return NULL;


	/* 333 <channel> <nick> <time> */
	case RPL_TOPICWHOTIME:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_TOPICWHOTIME: channel is null";

		if (!(nick = getarg(&p->params, 1)))
			return "RPL_TOPICWHOTIME: nick is null";

		if (!(time = getarg(&p->params, 1)))
			return "RPL_TOPICWHOTIME: time is null";

		if ((c = channel_get(chan, s)) == NULL)
			return errf("RPL_TOPICWHOTIME: channel '%s' not found", chan);

		time_t raw_time = atoi(time);
		time = ctime(&raw_time);

		newlinef(c, LINE_NUMRPL, "--", "Topic set by %s, %s", nick, time);
		return NULL;


	/* 353 ("="/"*"/"@") <channel> :*([ "@" / "+" ]<nick>) */
	case RPL_NAMREPLY:

		/* @:secret   *:private   =:public */
		if (!(type = getarg(&p->params, 1)))
			return "RPL_NAMEREPLY: type is null";

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_NAMEREPLY: channel is null";

		if ((c = channel_get(chan, s)) == NULL)
			return errf("RPL_NAMEREPLY: channel '%s' not found", chan);

		c->type = *type;

		while ((nick = getarg(&p->trailing, 1))) {
			if (*nick == '@' || *nick == '+')
				nick++;
			if (nicklist_insert(&c->nicklist, nick))
				c->nick_count++;
		}

		draw(D_STATUS);

		return NULL;


	case RPL_STATSCONN:    /* 250 :<Message> */
	case RPL_LUSERCLIENT:  /* 251 :<Message> */

		newline(s->channel, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_LUSEROP:        /* 252 <int> :IRC Operators online */
	case RPL_LUSERUNKNOWN:   /* 253 <int> :Unknown connections */
	case RPL_LUSERCHANNELS:  /* 254 <int> :Channels formed */

		if (!(num = getarg(&p->params, 1)))
			num = "NULL";

		newlinef(s->channel, LINE_NUMRPL, "--", "%s %s", num, p->trailing);
		return NULL;


	case RPL_LUSERME:      /* 255 :I have <int> clients and <int> servers */
	case RPL_LOCALUSERS:   /* 265 <int> <int> :Local users <int>, max <int> */
	case RPL_GLOBALUSERS:  /* 266 <int> <int> :Global users <int>, max <int> */
	case RPL_MOTD:         /* 372 : - <Message> */
	case RPL_MOTDSTART:    /* 375 :<server> Message of the day */

		newline(s->channel, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	/* Not printing these */
	case RPL_NOTOPIC:     /* 331 <chan> :<Message> */
	case RPL_ENDOFNAMES:  /* 366 <chan> :<Message> */
	case RPL_ENDOFMOTD:   /* 376 :<Message> */

		return NULL;


	default:

		newlinef(s->channel, LINE_NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

num_400:

	/* Numeric types (400, 600) */
	switch (code) {

	case ERR_CANNOTSENDTOCHAN:  /* <channel> :<reason> */

		if (!(chan = getarg(&p->params, 1)))
			return "ERR_CANNOTSENDTOCHAN: channel is null";

		channel *c;

		if ((c = channel_get(chan, s)))
			newline(c, LINE_NUMRPL, 0, p->trailing, 0);
		else
			newline(s->channel, LINE_NUMRPL, 0, p->trailing, 0);

		if (p->trailing)
			newlinef(c, LINE_NUMRPL, "--", "Cannot send to '%s' - %s", chan, p->trailing);
		else
			newlinef(c, LINE_NUMRPL, "--", "Cannot send to '%s'", chan);


	case ERR_ERRONEUSNICKNAME:  /* 432 <nick> :<reason> */

		if (!(nick = getarg(&p->params, 1)))
			return "ERR_ERRONEUSNICKNAME: nick is null";

		newlinef(s->channel, LINE_NUMRPL, "-!!-", "'%s' - %s", nick, p->trailing);
		return NULL;

	case ERR_NICKNAMEINUSE:  /* 433 <nick> :Nickname is already in use */

		newlinef(s->channel, LINE_NUMRPL, "-!!-", "Nick '%s' in use", s->nick_me);

		get_auto_nick(&(s->nptr), s->nick_me);

		newlinef(s->channel, LINE_NUMRPL, "-!!-", "Trying again with '%s'", s->nick_me);

		sendf(s, "NICK %s", s->nick_me);
		return NULL;


	default:

		newlinef(s->channel, LINE_NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

	return NULL;
}

char*
recv_part(parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain PART <channel> [:message] */

	char *targ;
	channel *c;

	if (!p->from)
		return "PART: sender's nick is null";

	if (IS_ME(p->from))
		return NULL;

	if (!(targ = getarg(&p->params, 1)))
		return "PART: target is null";

	if ((c = channel_get(targ, s)) && nicklist_delete(&c->nicklist, p->from)) {
		c->nick_count--;
		if (c->nick_count < config.join_part_quit_threshold) {
			if (p->trailing)
				newlinef(c, LINE_PART, "<", "%s left %s (%s)", p->from, targ, p->trailing);
			else
				newlinef(c, LINE_PART, "<", "%s left %s", p->from, targ);
		}
	}

	draw(D_STATUS);

	return NULL;
}

char*
recv_ping(parsed_mesg *p, server *s)
{
	/* PING :<server name> */

	if (!p->trailing)
		return "PING: servername is null";

	sendf(s, "PONG %s", p->trailing);

	return NULL;
}

char*
recv_priv(parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain PRIVMSG <target> :<message> */

	char *targ;

	if (!p->trailing)
		return "PRIVMSG: message is null";

	/* CTCP request */
	if (*p->trailing == 0x01)
		return recv_ctcp_req(p, s);

	if (!p->from)
		return "PRIVMSG: sender's nick is null";

	if (!(targ = getarg(&p->params, 1)))
		return "PRIVMSG: target is null";

	channel *c;

	if (IS_ME(targ)) {

		if ((c = channel_get(p->from, s)) == NULL) {
			c = new_channel(p->from, s, s->channel);
			c->type = 'p';
		}

		if (c != ccur)
			c->active = ACTIVITY_PINGED;

		draw(D_CHANS);

	} else if ((c = channel_get(targ, s)) == NULL)
		return errf("PRIVMSG: channel '%s' not found", targ);

	if (check_pinged(p->trailing, s->nick_me)) {

		if (c != ccur)
			c->active = ACTIVITY_PINGED;

		newline(c, LINE_PINGED, p->from, p->trailing, 0);
	} else {
		newline(c, 0, p->from, p->trailing, 0);
	}

	draw(D_CHANS);

	return NULL;
}

char*
recv_quit(parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain QUIT [:message] */

	if (!p->from)
		return "QUIT: sender's nick is null";

	channel *c = s->channel;
	do {
		if (nicklist_delete(&c->nicklist, p->from)) {
			c->nick_count--;
			if (c->nick_count < config.join_part_quit_threshold) {
				if (p->trailing)
					newlinef(c, LINE_QUIT, "<", "%s has quit (%s)", p->from, p->trailing);
				else
					newlinef(c, LINE_QUIT, "<", "%s has quit", p->from);
			}
		}
		c = c->next;
	} while (c != s->channel);

	draw(D_STATUS);

	return NULL;
}

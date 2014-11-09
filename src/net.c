/* Needed so gcc will use POSIX addrinfo and friends */
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
 * TODO: 403, :no such channel */

#define IS_ME(X) !strcmp(X, s[rplsoc]->nick_me)

/* Connection thread info */
typedef struct connection_thread {
	int socket;
	char *error;   /* Thread's error message */
	char *ipstr;   /* IPv4/6 address string */
	struct server *server;
	pthread_t tid;
} connection_thread;

/* Message receiving handlers */
char* recv_ctcp_req(parsed_mesg*);
char* recv_ctcp_rpl(parsed_mesg*);
char* recv_error(parsed_mesg*);
char* recv_join(parsed_mesg*);
char* recv_mode(parsed_mesg*);
char* recv_nick(parsed_mesg*);
char* recv_notice(parsed_mesg*);
char* recv_numeric(parsed_mesg*);
char* recv_part(parsed_mesg*);
char* recv_ping(parsed_mesg*);
char* recv_priv(parsed_mesg*);
char* recv_quit(parsed_mesg*);

/* Message sending handlers */
void send_connect(char*);
void send_default(char*);
void send_emote(char*);
void send_join(char*);
void send_nick(char*);
void send_part(char*);
void send_priv(char*);
void send_raw(char*);
void send_quit(char*);
void send_version(char*);

channel* get_channel(char*);
server* new_server(char*, char*);
void free_server(server *s);
void get_auto_nick(char**, char*);
void sendf(int, const char*, ...);
void recv_mesg(char*, int, server*);

/* TODO: remove these.. useless global state,
 * pass server* to recv handlers */
int rplsoc = 0;
server *s[MAXSERVERS + 3];

int numservers;
static struct pollfd pollfds[MAXSERVERS];
static struct server *servers[MAXSERVERS];

static void free_connection_info(connection_thread*);

static void server_connected(server*);
static void server_disconnect(server*);
static void server_disconnected(server*);

static void* threaded_connect(void*);
static void* threaded_connect_cleanup(void*);

void
server_connect(char *host, char *port)
{
	connection_thread *ct;

	if (numservers == MAXSERVERS) {
		newline(ccur, 0, "-!!-", "Error: MAXSERVERS", 0);
		return;
	}

	/* TODO: check for existing server mathing host:port
	 * before creating a new one */
	server *s = new_server(host, port);

	if ((ct = malloc(sizeof(connection_thread))) == NULL)
		fatal("server_connect - malloc");

	s->connecting = ct;

	ct->error = NULL;
	ct->ipstr = NULL;
	ct->socket = -1;
	ct->server = s;

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

	newlinef(s->channel, 0, "--", "Connected [%s]", ct->ipstr);

	s->soc = ct->socket;

	sendf(ct->socket, "NICK %s\r\n", ct->server->nick_me);
	sendf(ct->socket, "USER %s 8 * :%s\r\n", config.username, config.realname);
}

static void*
threaded_connect(void *arg)
{
	void *addr;
	int ret, soc;
	char ipstr[INET6_ADDRSTRLEN];
	struct addrinfo hints, *p, *servinfo = NULL;

	/* Thread cleanup on error or external thread cancel */
	pthread_cleanup_push(threaded_connect_cleanup, servinfo);

	memset(&hints, 0, sizeof(hints));

	/* IPv4 and/or IPv6 */
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	connection_thread *ct = (connection_thread *)arg;

	server *s = ct->server;

	/* Resolve host */
	if ((ret = getaddrinfo(s->host, s->port, &hints, &servinfo)) != 0) {
		ct->error = strdupf("Error resolving host: %s", gai_strerror(ret));
		pthread_exit(NULL);
	}

	/* Attempt to connect to all address results */
	for (p = servinfo; p != NULL; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		/* Break on success */
		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0)
			break;

		close(soc);
	}

	/* Check for failure to connect */
	if (p == NULL) {
		ct->error = strdupf("Error connecting: %s", strerror(errno));
		pthread_exit(NULL);
	}

	/* Get IPv4 or IPv6 address string */
	if (p->ai_family == AF_INET)
		addr = &(((struct sockaddr_in *)p->ai_addr)->sin_addr);
	else
		addr = &(((struct sockaddr_in6 *)p->ai_addr)->sin6_addr);

	ct->ipstr = strdup(inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr)));
	ct->socket = soc;

	pthread_cleanup_pop(1);

	return NULL;
}

static void*
threaded_connect_cleanup(void* arg)
{
	struct addrinfo *servinfo = (struct addrinfo *)arg;

	if (servinfo)
		freeaddrinfo(servinfo);

	return NULL;
}

static void
free_connection_info(connection_thread *ct)
{
	free(ct->error);
	free(ct->ipstr);
	free(ct);
}

void
check_servers(void)
{
	/* FIXME:
	 * this whole function is a mess while i rewrite how servers are polled
	 * to remove server limit restriction and make things more flexible in
	 * general. Keeping server ordered as global state is becoming messy
	 * and problematic. Polling servers individually is a slight
	 * performance hit but cleans things up significantly.
	 * Once all the connected stuff is proven to work perfectly
	 * this can be revisited.
	 * */

	int i, count;
	char buff[BUFFSIZE];

	for (i = 0; i < numservers; i++) {
		if (servers[i]->connecting) {

			connection_thread *ct = (connection_thread*) servers[i]->connecting;

			if (ct->error) {
				newline(servers[i]->channel, 0, "-!!-", ct->error, 0);
				free_connection_info(ct);
				servers[i]->connecting = NULL;
			}

			if (ct->socket >= 0) {
				server_connected(servers[i]);
				free_connection_info(ct);
				servers[i]->connecting = NULL;
			}
		}
	}

	/* TODO: iterate over servers and poll individually to eliminate
	 * the need to keep fds and servers ordered
	 * temp: repacking FDs on every call */
	for (i = 0; i < numservers; i++) {
		pollfds[i].fd = servers[i]->soc;
		pollfds[i].events = POLLIN;
	}

	if (poll(pollfds, numservers, 0)) {
		for (i = 0; i < numservers; i++) {
			if (pollfds[i].revents & POLLIN) {
				if ((count = read(pollfds[i].fd, buff, BUFFSIZE)) <= 0)
					server_disconnected(servers[i]);
				else {
					/* FIXME: temp. removing rplsoc next */
					s[servers[i]->soc] = servers[i];

					recv_mesg(buff, count, servers[i]);
				}

				count = 0;
			}
		}
	}
}

/* FIXME: this is an infinite loop when rirc exists because
 * we're not freeing the servers */
static void
server_disconnect(server *s)
{
	/* TODO: make sure thread cancellation closes
	 * any sockets. See: pthread cancellation points.
	 * man 7 pthreads */
	if (s->connecting) {
		/* Connection thread in progress, cancel the pthread */

		connection_thread *ct = s->connecting;

		newlinef(s->channel, 0, "--", "Connection to '%s' port %s canceled",
				s->host, s->port);

		if ((pthread_cancel(ct->tid)))
			fatal("threaded_connect_cancel - pthread_cancel");

		free_connection_info(ct);

		s->connecting = NULL;
	} else if (s->soc >= 0) {

		channel *c = cfirst;

		sendf(ccur->server->soc, "QUIT :rirc v"VERSION"\r\n");

		do {
			if (c->server == s)
				newline(c, 0, "-!!-", "(disconnected)", 0);
			c = c->next;
		} while (c != cfirst);

		close(s->soc);

		s->soc = -1;

		/* TODO
		 *	set server/user/channel modes to 0?
		 *	reset nick_ptr? or is that done elsewhere
		 *	etc
		 * */
	}
}

static void
server_disconnected(server *s)
{
	;
	/* TODO: remote connection closed/lost.
	 * Similar to server_disconnect, but:
	 *	dont send a quit message
	 *	print error message
	 *	we never kill the server in this case */
}

/* FIXME: some clients can actually send a null message.
 * this messes up the printing process by thinking there
 * are no more lines to print and effectively 'clears' the screen */
/* XXX: the routine that prints should check if the POINTER is null,
 * not the contents of the pointer, thats how we know for sure that
 * its an empty message */
void
newline(channel *c, line_t type, char *from, char *mesg, int len)
{
	time_t raw_t;
	struct tm *t;

	if (len == 0)
		len = strlen(mesg);

	if (c == 0) {
		if (cfirst == rirc)
			c = rirc;
		else
			/* FIXME: with the new servers[] buffer, wrong,
			 * might not even be possible to use this distinction
			 * anymore. probably for the best */
			/* XXX: maybe newline with no channel shouldnt be possible... */
			c = s[rplsoc]->channel;
	}

	line *l = c->cur_line++;

	free(l->text);

	l->len = len;
	if ((l->text = malloc(len)) == NULL)
		fatal("newline");
	memcpy(l->text, mesg, len);

	time(&raw_t);
	t = localtime(&raw_t);
	l->time_h = t->tm_hour;
	l->time_m = t->tm_min;

	l->type = type;

	if (!from) /* Server message */
		strncpy(l->from, c->name, 50);
	else
		strncpy(l->from, from, 50);

	int len_from;
	if ((len_from = strlen(l->from)) > c->nick_pad)
		c->nick_pad = len_from;

	if (c->cur_line == &c->chat[SCROLLBACK_BUFFER])
		c->cur_line = c->chat;

	if (c == ccur)
		draw(D_CHAT);
	else if (!type && c->active < ACTIVITY_ACTIVE) {
		c->active = ACTIVITY_ACTIVE;
		draw(D_CHANS);
	}
}

void
newlinef(channel *c, line_t type, char *from, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buff[BUFFSIZE];
	int len = vsnprintf(buff, BUFFSIZE-1, fmt, args);
	newline(c, type, from, buff, len);
	va_end(args);
}

void
sendf(int soc, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buff[BUFFSIZE];
	int len = vsnprintf(buff, BUFFSIZE-1, fmt, args);
	/* TODO: check for error */
	send(soc, buff, len, 0);
	va_end(args);
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
		char *cset = "0123456789";

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

	if (ccur == rirc)
		ccur = NULL;

	s->channel = new_channel(host, s);

	servers[numservers++] = s;

	return s;
}

channel*
new_channel(char *name, server *server)
{
	channel *c;
	if ((c = malloc(sizeof(channel))) == NULL)
		fatal("new_channel");

	c->type = '\0';
	c->nick_pad = 0;
	c->chanmode = 0;
	c->nick_count = 0;
	c->nicklist = NULL;
	c->server = server;
	c->cur_line = c->chat;
	c->active = ACTIVITY_DEFAULT;
	c->input = new_input();
	strncpy(c->name, name, 50);
	memset(c->chat, 0, sizeof(c->chat));

	if (ccur == NULL) {
		cfirst = ccur = c;
		c->prev = c->next = c;
	} else {
		/* Skip to end of server channels */
		while (!ccur->next->type && ccur->next != cfirst)
			ccur = ccur->next;
		c->prev = ccur;
		c->next = ccur->next;
		ccur->next->prev = c;
		ccur->next = c;
	}

	draw(D_CHANS);

	return c;
}

void
free_server(server *s)
{
	/* TODO: does this have to check if the server has
	 * a connection in progress? */
	int i;
	for (i = 0; i < numservers; i++)
		if (s == servers[i])
			break;

	/* Shuffle the server/socket lists */
	servers[i] = servers[numservers--];

	/* TODO: free all channels that point to this server */
	/* TODO: update ccur if applicable */
	/* TODO: actually call this function when killing a server */

	free(s->host);
	free(s->port);
	free(s);
}

void
free_channel(channel *c)
{
	/* Remove from linked list */
	c->next->prev = c->prev;
	c->prev->next = c->next;

	/* Free all chat lines and everything else */
	line *l = c->chat;
	line *e = l + SCROLLBACK_BUFFER;
	while (l->len && l < e)
		free((l++)->text);
	free_nicklist(c->nicklist);
	free_input(c->input);
	free(c);

	draw(D_CHANS);
}

channel*
get_channel(char *chan)
{
	channel *c = cfirst;

	do {
		if (!strcmp(c->name, chan) && c->server->soc == rplsoc)
			return c;
		c = c->next;
	} while (c != cfirst);

	return NULL;
}

void
channel_close(void)
{
	if (!ccur->type) {
		server_disconnect(ccur->server);
	} else {
		channel *c = ccur;
		sendf(ccur->server->soc, "PART %s\r\n", ccur->name);
		ccur = (ccur->next == cfirst) ? ccur->prev : ccur->next;
		free_channel(c);
	}

	draw(D_FULL);
}

void
channel_switch(int next)
{
	if (ccur->next == ccur)
		return;
	else if (next)
		ccur = ccur->next;
	else
		ccur = ccur->prev;

	ccur->active = ACTIVITY_DEFAULT;

	draw(D_FULL);
}

/*
 * Message sending handlers
 * */

void
send_mesg(char *mesg)
{
	char *cmd;

	if (*mesg != '/')
		send_default(mesg);
	else {
		mesg++;
		if (!(cmd = getarg(&mesg, 1)))
			; /* message == "/", do nothing */
		else if (!strcasecmp(cmd, "JOIN"))
			send_join(mesg);
		else if (!strcasecmp(cmd, "CONNECT"))
			send_connect(mesg);
		else if (!strcasecmp(cmd, "DISCONNECT"))
			server_disconnect(ccur->server);
		else if (!strcasecmp(cmd, "CLOSE"))
			channel_close();
		else if (!strcasecmp(cmd, "PART"))
			send_part(mesg);
		else if (!strcasecmp(cmd, "NICK"))
			send_nick(mesg);
		else if (!strcasecmp(cmd, "QUIT"))
			send_quit(mesg);
		else if (!strcasecmp(cmd, "MSG"))
			send_priv(mesg);
		else if (!strcasecmp(cmd, "PRIV"))
			send_priv(mesg);
		else if (!strcasecmp(cmd, "ME"))
			send_emote(mesg);
		else if (!strcasecmp(cmd, "VERSION"))
			send_version(mesg);
		else if (!strcasecmp(cmd, "RAW"))
			send_raw(mesg);
		else {
			int len = strlen(cmd);
			newlinef(ccur, 0, "-!!-", "Unknown command: %.*s%s",
					15, cmd, len > 15 ? "..." : "");
		}
	}
}

void
send_connect(char *ptr)
{
	char *host, *port;

	/* Accept <host> || <host:port> || <hostport> */
	if (!(host= strtok(ptr, " :"))) {

		/* TODO: if current server is disconnected, attempt to reconnect
		 * if no arguments are given*/

		newline(ccur, 0, "-!!-", "connect requires a hostname argument", 0);
		return;
	}

	/* Check for port */
	if (!(port = strtok(NULL, " ")))
		port = "6667";

	server_connect(host, port);
}

void
send_default(char *mesg)
{
	if (!ccur->type)
		newline(ccur, 0, "-!!-", "This is not a channel!", 0);
	else if (ccur->server->soc < 0)
		newline(ccur, 0, "-!!-", "Disconnected from server", 0);
	else {
		newline(ccur, 0, ccur->server->nick_me, mesg, 0);
		sendf(ccur->server->soc, "PRIVMSG %s :%s\r\n", ccur->name, mesg);
	}
}

void
send_emote(char *ptr)
{
	if (!ccur->type)
		newline(ccur, 0, "-!!-", "This is not a channel!", 0);
	else if (ccur->server->soc < 0)
		newline(ccur, 0, "-!!-", "Disconnected from server", 0);
	else {
		newlinef(ccur, LINE_ACTION, "*", "%s %s", ccur->server->nick_me, ptr);
		sendf(ccur->server->soc,
				"PRIVMSG %s :\x01""ACTION %s\x01""\r\n", ccur->name, ptr);
	}
}

void
send_join(char *ptr)
{
	if (ccur == rirc)
		newline(ccur, 0, "-!!-", "Cannot execute 'join' on main buffer", 0);
	else if (ccur->server->soc < 0)
		newline(ccur, 0, "-!!-", "Disconnected from server", 0);
	else
		sendf(ccur->server->soc, "JOIN %s\r\n", ptr);
}

void
send_nick(char *ptr)
{
	if (ccur == rirc)
		newline(ccur, 0, "-!!-", "Cannot execute 'nick' on main buffer", 0);
	else if (ccur->server->soc < 0)
		newline(ccur, 0, "-!!-", "Disconnected from server", 0);
	else
		sendf(ccur->server->soc, "NICK %s\r\n", ptr);
}

void
send_part(char *ptr)
{
	/* TODO: part message from ptr */

	if (!ccur->type)
		newline(ccur, 0, "-!!-", "This is not a channel!", 0);
	else if (ccur->server->soc < 0)
		newline(ccur, 0, "-!!-", "Disconnected from server", 0);
	else {
		/* TODO: this should set a 'parted' flag for this channel.
		 * Users should be able to send to a parted channel.
		 * '/join' with no argument should attemp to rejoin this channel
		 * if parted. recv_join should look for open channels with that name
		 * before opening a new one */
		newline(ccur, 0, "--", "(disconnected)", 0);
		sendf(ccur->server->soc, "PART %s\r\n", ccur->name);
	}
}

void
send_priv(char *ptr)
{
	char *targ;
	channel *c;

	if (ccur == rirc) {
		newline(ccur, 0, "-!!-", "Cannot send messages on main buffer", 0);
		return;
	} else if (!(targ = getarg(&ptr, 1))) {
		newline(ccur, 0, "-!!-", "Private messages require a target", 0);
		return;
	} else if (*ptr == '\0') {
		newline(ccur, 0, "-!!-", "Private message was null", 0);
		return;
	}

	if ((c = get_channel(targ)))
		newline(c, 0, ccur->server->nick_me, ptr, 0);
	/* else: */
		/* TODO: should a new channel be created as soon as I send this?
		 * or should it be printed to ccur with a markup to denote its
		 * being sent to a new channel? */

	sendf(c->server->soc, "PRIVMSG %s :%s\r\n", targ, ptr);
}

void
send_raw(char *ptr)
{
	if (ccur == rirc)
		newline(ccur, 0, "-!!-", "Cannot execute 'raw' on main buffer", 0);
	else
		sendf(ccur->server->soc, "%s\r\n", ptr);
}

void
send_quit(char *ptr)
{
	/* XXX: exit calls cleanup() which frees everything
	 * and sends a default quit message to each server.
	 *
	 * It should be possible to take a message from ptr and
	 * use that instead of the default quit message. */

	/* /quit is the only time we clear the screen: */
	/* TODO: there's a simpler form of this that clears the entire screen? */
	/* Should actually just 'restore screen' to before rirc was invoked */
	printf("\x1b[H\x1b[J");

	exit(EXIT_SUCCESS);
}

void
send_version(char *ptr)
{
	char *targ;

	if (ccur == rirc) {
		newline(ccur, 0, "--", "rirc version " VERSION, 0);
		newline(ccur, 0, "--", "http://rcr.io/rirc.html", 0);
	} else if ((targ = getarg(&ptr, 1))) {
		newlinef(ccur, 0, "--", "Sending CTCP VERSION request to %s", targ);
		sendf(ccur->server->soc, "PRIVMSG %s :\x01""VERSION\x01""\r\n", targ);
	} else {
		sendf(ccur->server->soc, "VERSION\r\n");
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

	while (count--) {
		if (*inp == '\r') {

			*ptr = '\0';

			rplsoc = s->soc;

			parsed_mesg *p;

			char *err = NULL;
			if (!(p = parse(s->input)))
				err = "Failed to parse message";
			else if (isdigit(*p->command))
				err = recv_numeric(p);
			else if (!strcmp(p->command, "PRIVMSG"))
				err = recv_priv(p);
			else if (!strcmp(p->command, "JOIN"))
				err = recv_join(p);
			else if (!strcmp(p->command, "PART"))
				err = recv_part(p);
			else if (!strcmp(p->command, "QUIT"))
				err = recv_quit(p);
			else if (!strcmp(p->command, "NOTICE"))
				err = recv_notice(p);
			else if (!strcmp(p->command, "NICK"))
				err = recv_nick(p);
			else if (!strcmp(p->command, "PING"))
				err = recv_ping(p);
			else if (!strcmp(p->command, "MODE"))
				err = recv_mode(p);
			else if (!strcmp(p->command, "ERROR"))
				err = recv_error(p);
			else
				err = errf("Message type '%s' unknown", p->command);

			if (err)
				newline(0, 0, "-!!-", err, 0);

			ptr = s->input;

		/* Don't accept unprintable characters unless space or ctcp markup */
		} else if (ptr < max && (isgraph(*inp) || *inp == ' ' || *inp == 0x01))
			*ptr++ = *inp;

		inp++;
	}

	s->iptr = ptr;
}

char*
recv_ctcp_req(parsed_mesg *p)
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
		if ((c = get_channel(p->from)) == NULL) {
			c = new_channel(p->from, s[rplsoc]);
			c->type = 'p';
		}

		newlinef(c, 0, "*", "%s %s", p->from, p->trailing);
		return NULL;
	}

	if (!strcmp(cmd, "VERSION")) {

		if ((c = get_channel(p->from)) == NULL)
			c = s[rplsoc]->channel;

		newlinef(c, 0, "--", "Received CTCP VERSION from %s", p->from);
		sendf(rplsoc, "NOTICE %s :\x01""VERSION rirc version %s\x01""\r\n", p->from, VERSION);
		sendf(rplsoc, "NOTICE %s :\x01""VERSION http://rcr.io/rirc.html\x01""\r\n", p->from);
		return NULL;
	}

	sendf(rplsoc, "NOTICE %s :\x01""ERRMSG %s\x01""\r\n", p->from, cmd);
	return errf("CTCP: unknown command '%s'", cmd);
}

char*
recv_ctcp_rpl(parsed_mesg *p)
{
	/* CTCP replies:
	 * NOTICE <target> :0x01<command> <arguments>0x01 */

	/* TODO */
	return NULL;
}

char*
recv_error(parsed_mesg *p)
{
	/* TODO */
	return NULL;
}

char*
recv_join(parsed_mesg *p)
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
		ccur = new_channel(chan, s[rplsoc]);
		draw(D_FULL);
	} else {

		if ((c = get_channel(chan)) == NULL)
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
recv_mode(parsed_mesg *p)
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
	if ((c = get_channel(targ))) {

		newlinef(c, 0, "--", "%s chanmode: [%s]", targ, flags);

		int *chanmode = &c->chanmode;

		/* Chanmodes */
		do {
			if (*flags == '+' || *flags == '-')
				plusminus = *flags;
			else if (plusminus == '\0') {
				return "MODE: +/- flag is null";
			} else {
				switch(*flags) {
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
						newlinef(0, 0, "-!!-", "Unknown mode '%c'", *flags);
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

		newlinef(0, 0, "--", "%s usermode: [%s]", targ, flags);

		int *usermode = &s[rplsoc]->usermode;

		/* Usermodes */
		do {
			if (*flags == '+' || *flags == '-')
				plusminus = *flags;
			else if (plusminus == '\0') {
				return "MODE: +/- flag is null";
			} else {
				switch(*flags) {
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
						newlinef(0, 0, "-!!-", "Unknown mode '%c'", *flags);
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
recv_nick(parsed_mesg *p)
{
	/* :nick!user@hostname.domain NICK [:]<new nick> */

	char *nick;

	if (!p->from)
		return "NICK: old nick is null";

	/* Check params first, then trailing */
	if (!(nick = getarg(&p->params, 1)) && !(nick = getarg(&p->trailing, 1)))
		return "NICK: new nick is null";

	if (IS_ME(p->from)) {
		strncpy(s[rplsoc]->nick_me, nick, NICKSIZE-1);
		newlinef(0, 0, "--", "You are now known as %s", nick);
	}

	channel *c = cfirst;

	do {
		if (c->server == s[rplsoc] && nicklist_delete(&c->nicklist, p->from)) {
			nicklist_insert(&c->nicklist, nick);
			newlinef(c, LINE_NICK, "--", "%s  >>  %s", p->from, nick);
		}
		c = c->next;
	} while (c != cfirst);

	return NULL;
}

char*
recv_notice(parsed_mesg *p)
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

	if ((c = get_channel(targ)))
		newline(c, 0, 0, p->trailing, 0);
	else
		newline(0, 0, 0, p->trailing, 0);

	return NULL;
}

char*
recv_numeric(parsed_mesg *p)
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
		s[rplsoc]->nptr = config.nicks;

		/* Only send the autojoin on command-line connect */
		if (config.auto_join) {
			sendf(rplsoc, "JOIN %s\r\n", config.auto_join);
			config.auto_join = NULL;
		}

		newline(0, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_YOURHOST:  /* 002 <nick> :<Host info, server version, etc> */
	case RPL_CREATED:   /* 003 <nick> :<Server creation date message> */

		newline(0, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_MYINFO:    /* 004 <nick> <params> :Are supported by this server */
	case RPL_ISUPPORT:  /* 005 <nick> <params> :Are supported by this server */

		newlinef(0, LINE_NUMRPL, "--", "%s ~ %s", p->params, p->trailing);
		return NULL;


	default:

		newlinef(0, LINE_NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

num_200:

	/* Numeric types (200, 400) */
	switch (code) {

	/* 328 <channel> :<url> */
	case RPL_CHANNEL_URL:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_CHANNEL_URL: channel is null";

		if ((c = get_channel(chan)) == NULL)
			return errf("RPL_CHANNEL_URL: channel '%s' not found", chan);

		newlinef(c, LINE_NUMRPL, "--", "URL for %s is: \"%s\"", chan, p->trailing);
		return NULL;


	/* 332 <channel> :<topic> */
	case RPL_TOPIC:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_TOPIC: channel is null";

		if ((c = get_channel(chan)) == NULL)
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

		if ((c = get_channel(chan)) == NULL)
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

		if ((c = get_channel(chan)) == NULL)
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

		newline(0, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_LUSEROP:        /* 252 <int> :IRC Operators online */
	case RPL_LUSERUNKNOWN:   /* 253 <int> :Unknown connections */
	case RPL_LUSERCHANNELS:  /* 254 <int> :Channels formed */

		if (!(num = getarg(&p->params, 1)))
			num = "NULL";

		newlinef(0, LINE_NUMRPL, "--", "%s %s", num, p->trailing);
		return NULL;


	case RPL_LUSERME:       /* 255 :I have <int> clients and <int> servers */
	case RPL_LOCALUSERS:    /* 265 <int> <int> :Local users <int>, max <int> */
	case RPL_GLOBALUSERS:   /* 266 <int> <int> :Global users <int>, max <int> */
	case RPL_MOTD:          /* 372 : - <Message> */
	case RPL_MOTDSTART:     /* 375 :<server> Message of the day */

		newline(0, LINE_NUMRPL, "--", p->trailing, 0);
		return NULL;


	/* Not printing these */
	case RPL_NOTOPIC:     /* 331 <chan> :<Message> */
	case RPL_ENDOFNAMES:  /* 366 <chan> :<Message> */
	case RPL_ENDOFMOTD:   /* 376 :<Message> */

		return NULL;


	default:

		newlinef(0, LINE_NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

num_400:

	/* Numeric types (400, 600) */
	switch (code) {

	case ERR_CANNOTSENDTOCHAN:  /* <channel> :<reason> */

		if (!(chan = getarg(&p->params, 1)))
			return "ERR_CANNOTSENDTOCHAN: channel is null";

		channel *c;

		if ((c = get_channel(chan)))
			newline(c, LINE_NUMRPL, 0, p->trailing, 0);
		else
			newline(0, LINE_NUMRPL, 0, p->trailing, 0);

		if (p->trailing)
			newlinef(c, LINE_NUMRPL, "--", "Cannot send to '%s' - %s", chan, p->trailing);
		else
			newlinef(c, LINE_NUMRPL, "--", "Cannot send to '%s'", chan);


	case ERR_ERRONEUSNICKNAME:  /* 432 <nick> :Erroneous nickname */

		if (!(nick = getarg(&p->params, 1)))
			return "ERR_ERRONEUSNICKNAME: nick is null";

		newlinef(0, LINE_NUMRPL, "-!!-", "Erroneous nickname: '%s'", nick);
		return NULL;

	case ERR_NICKNAMEINUSE:  /* 433 <nick> :Nickname is already in use */

		newlinef(0, LINE_NUMRPL, "-!!-", "Nick '%s' in use", s[rplsoc]->nick_me);

		get_auto_nick(&(s[rplsoc]->nptr), s[rplsoc]->nick_me);

		newlinef(0, LINE_NUMRPL, "-!!-", "Trying again with '%s'", s[rplsoc]->nick_me);

		sendf(rplsoc, "NICK %s\r\n", s[rplsoc]->nick_me);
		return NULL;


	default:

		newlinef(0, LINE_NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

	return NULL;
}

char*
recv_part(parsed_mesg *p)
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

	if ((c = get_channel(targ)) && nicklist_delete(&c->nicklist, p->from)) {
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
recv_ping(parsed_mesg *p)
{
	/* PING :<server name> */

	if (!p->trailing)
		return "PING: servername is null";

	sendf(rplsoc, "PONG %s\r\n", p->trailing);

	return NULL;
}

char*
recv_priv(parsed_mesg *p)
{
	/* :nick!user@hostname.domain PRIVMSG <target> :<message> */

	char *targ;

	if (!p->trailing)
		return "PRIVMSG: message is null";

	/* CTCP request */
	if (*p->trailing == 0x01)
		return recv_ctcp_req(p);

	if (!p->from)
		return "PRIVMSG: sender's nick is null";

	if (!(targ = getarg(&p->params, 1)))
		return "PRIVMSG: target is null";

	channel *c;

	if (IS_ME(targ)) {

		if ((c = get_channel(p->from)) == NULL) {
			c = new_channel(p->from, s[rplsoc]);
			c->type = 'p';
		}

		if (c != ccur)
			c->active = ACTIVITY_PINGED;

		draw(D_CHANS);

	} else if ((c = get_channel(targ)) == NULL)
		return errf("PRIVMSG: channel '%s' not found", targ);

	if (check_pinged(p->trailing, s[rplsoc]->nick_me)) {

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
recv_quit(parsed_mesg *p)
{
	/* :nick!user@hostname.domain QUIT [:message] */

	channel *c = cfirst;

	if (!p->from)
		return "QUIT: sender's nick is null";

	do {
		if (c->server == s[rplsoc] && nicklist_delete(&c->nicklist, p->from)) {
			c->nick_count--;
			if (c->nick_count < config.join_part_quit_threshold) {
				if (p->trailing)
					newlinef(c, LINE_QUIT, "<", "%s has quit (%s)", p->from, p->trailing);
				else
					newlinef(c, LINE_QUIT, "<", "%s has quit", p->from);
			}
		}
		c = c->next;
	} while (c != cfirst);

	draw(D_STATUS);

	return NULL;
}

/* TODO:
 * net.c should be stateless,
 * shouldnt include state.h
 * shouldnt be calling newline, new_channel, auto_nick, etc
 * */

/* For addrinfo, getaddrinfo, getnameinfo */
#define _POSIX_C_SOURCE 200112L

#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __FreeBSD__
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
#endif

#include "src/comps/buffer.h"
#include "src/comps/mode.h"
#include "src/net.h"
#include "src/state.h"
#include "src/utils.h"

#define SERVER_TIMEOUT_S 255 /* Latency time at which a server is considered to be timed out and a disconnect is issued */
#define SERVER_LATENCY_S 125 /* Latency time at which to begin showing in the status bar */
#define SERVER_LATENCY_PING_S (SERVER_LATENCY_S - 10) /* Latency time at which to issue a PING before displaying latency */

#define RECONNECT_DELTA 15

#if SERVER_TIMEOUT_S <= SERVER_LATENCY_S
	#error Server timeout must be greater than latency counting time
#endif

#if SERVER_LATENCY_PING_S <= 0
	#error Server latency display time too low
#endif

/* Connection thread info */
typedef struct connection_thread {
	int socket;
	int socket_tmp;
	char *host;
	char *port;
	char error[MAX_ERROR + 1];
	char ipstr[INET6_ADDRSTRLEN];
	pthread_t tid;
} connection_thread;

/* DLL of current servers */
static struct server *server_head;

static struct server* new_server(char*, char*, char*, char*);
static void free_server(struct server*);

static int check_connect(struct server*);
static int check_latency(struct server*, time_t);
static int check_reconnect(struct server*, time_t);
static int check_socket(struct server*, time_t);

static void connected(struct server*);

static void* threaded_connect(void*);
static void threaded_connect_cleanup(void*);

/* FIXME: reorganize, this is a temporary fix in order to retrieve
 * the first/last channels for drawing purposes. */
struct server*
get_server_head(void)
{
	return server_head;
}

static struct server*
new_server(char *host, char *port, char *join, char *nicks)
{
	/* FIXME: refactor connections out of server, use server constructor
	 * from server.c*/

	struct server *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc", errno);

	/* Set non-zero default fields */
	s->soc = -1;
	s->iptr = s->input;
	s->host = strdup(host);
	s->port = strdup(port);

	if (nicks)
		s->nicks = strdup(nicks);
	else if (config.default_nick)
		s->nicks = strdup(config.default_nick);

	if (join)
		s->join = strdup(join);

	s->nptr = s->nicks;

	auto_nick(&(s->nptr), s->nick);

	s->channel = new_channel(host, s, NULL, BUFFER_SERVER);

	s->usermodes_str.type = MODE_STR_USERMODE;
	mode_config(&(s->mode_config), NULL, MODE_CONFIG_DEFAULTS);

	DLL_ADD(server_head, s);

	return s;
}

static void
free_server(struct server *s)
{
	struct channel *t, *c = s->channel;

	do {
		t = c;
		c = c->next;
		free_channel(t);
	} while (c != s->channel);

	free(s->host);
	free(s->port);
	free(s->join);
	free(s->nicks);
	free(s);
}

int
sendf(char *err, struct server *s, const char *fmt, ...)
{
	/* Send a formatted message to a server.
	 *
	 * Returns non-zero on failure and prints the error message to the buffer pointed
	 * to by err.
	 */

#define SENDF_ERR(str) \
	do { if (err) { snprintf(err, MAX_ERROR, "Error: %s", str); }  return 1; } while (0)

	char sendbuff[BUFFSIZE];
	int soc, len;
	va_list ap;

	if (s == NULL || (soc = s->soc) < 0)
		SENDF_ERR("Not connected to server");


	va_start(ap, fmt);
	len = vsnprintf(sendbuff, BUFFSIZE-2, fmt, ap);
	va_end(ap);

	if (len < 0)
		SENDF_ERR("Invalid message format");

	if (len >= BUFFSIZE-2)
		SENDF_ERR("Message exceeds maximum length of " STR(BUFFSIZE) " bytes");

#ifdef DEBUG
	newline(s->channel, 0, "DEBUG >>", sendbuff);
#endif

	sendbuff[len++] = '\r';
	sendbuff[len++] = '\n';

	if (send(soc, sendbuff, len, 0) < 0)
		SENDF_ERR(strerror(errno));

#undef SENDF_ERR

	return 0;
}

//FIXME: move the stateful stuff to state.c, only the connection relavent stuff should be here
void
server_connect(char *host, char *port, char *nicks, char *join)
{
	connection_thread *ct;
	struct server *tmp, *s = NULL;

	/* Check if server matching host:port already exists */
	if ((tmp = server_head) != NULL) {
		do {
			if (!strcmp(tmp->host, host) && !strcmp(tmp->port, port)) {
				s = tmp;
				break;
			}
		} while ((tmp = tmp->next) != server_head);
	}

	/* Check if server is already connected */
	if (s && s->soc >= 0) {
		channel_set_current(s->channel);
		newlinef(s->channel, 0, "-!!-", "Already connected to %s:%s", host, port);
		return;
	}

	if (s == NULL)
		s = new_server(host, port, join, nicks);

	channel_set_current(s->channel);

	if ((ct = calloc(1, sizeof(*ct))) == NULL)
		fatal("calloc", errno);

	ct->socket = -1;
	ct->socket_tmp = -1;
	ct->host = s->host;
	ct->port = s->port;

	s->connecting = ct;

	newlinef(s->channel, 0, "--", "Connecting to '%s' port %s", host, port);

	if ((errno = pthread_create(&ct->tid, NULL, threaded_connect, ct)))
		fatal("pthread_create", errno);
}

static void
connected(struct server *s)
{
	/* Server successfully connected, send IRC init messages */

	connection_thread *ct = s->connecting;

	if ((errno = pthread_join(ct->tid, NULL)))
		fatal("pthread_join", errno);

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

	//TODO: refactor these to mesg.c
	sendf(NULL, s, "NICK %s", s->nick);
	sendf(NULL, s, "USER %s 8 * :%s", config.username, config.realname);

	//FIXME: should the server send nick as is? compare the nick when it's received?
	//or should auto_nick take a server argument and write to a buffer of NICKSIZE length?
}

/* TODO: on failed connection, initiate rolling backoff reconnection attempt */
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

	/* coverity[leaked_storage] No leak here */
	return NULL;
}

static void
threaded_connect_cleanup(void *arg)
{
	struct addrinfo *servinfo = *(struct addrinfo **)arg;

	if (servinfo)
		freeaddrinfo(servinfo);
}

void
server_disconnect(struct server *s, int err, int kill, char *mesg)
{
	/* When err flag is set:
	 *   Disconnect initiated by remote host
	 *
	 * When kill flag is set:
	 *   Free the server, update current channel
	 */

	/* Server connection in progress, cancel the connection attempt */
	if (s->connecting) {

		connection_thread *ct = s->connecting;

		if ((errno = pthread_cancel(ct->tid)))
			fatal("pthread_cancel", errno);

		/* There's a chance the thread is canceled with an open socket */
		if (ct->socket_tmp)
			close(ct->socket_tmp);

		free(ct);
		s->connecting = NULL;

		newlinef(s->channel, 0, "--", "Connection to '%s' port %s canceled", s->host, s->port);
	}

	/* Server is/was connected, close socket, send quit message if non-erroneous disconnect */
	else if (s->soc >= 0) {

		if (err) {
			/* If disconnecting due to error, attempt a reconnect */

			newlinef(s->channel, 0, "ERROR", "%s", mesg);
			newlinef(s->channel, 0, "--", "Attempting reconnect in %ds", RECONNECT_DELTA);

			s->reconnect_time = time(NULL) + RECONNECT_DELTA;
			s->reconnect_delta = RECONNECT_DELTA;
		} else if (mesg) {
			sendf(NULL, s, "QUIT :%s", mesg);
		}

		close(s->soc);

		/* Set all server attributes back to default */
		mode_reset(&(s->usermodes), &(s->usermodes_str));
		s->soc = -1;
		s->iptr = s->input;
		s->nptr = s->nicks;
		s->latency_delta = 0;


		/* Reset the nick that reconnects will attempt to register with */
		auto_nick(&(s->nptr), s->nick);

		/* Print message to all open channels and reset their attributes */
		struct channel *c = s->channel;
		do {
			newline(c, 0, "-!!-", "(disconnected)");

			reset_channel(c);

		} while ((c = c->next) != s->channel);
	}

	/* Server was waiting to reconnect, cancel future attempt */
	else if (s->reconnect_time) {
		newlinef(s->channel, 0, "--", "Auto reconnect attempt canceled");

		s->reconnect_time = 0;
		s->reconnect_delta = 0;
	}

	if (kill) {
		DLL_DEL(server_head, s);
		free_server(s);
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

	struct server *s;

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
check_connect(struct server *s)
{
	/* Check the server's connection thread status for success or failure */

	if (!s->connecting)
		return 0;

	connection_thread *ct = (connection_thread*)s->connecting;

	/* Connection Success */
	if (ct->socket >= 0) {
		connected(s);

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
check_latency(struct server *s, time_t t)
{
	/* Check time since last message */

	time_t delta;

	if (s->soc < 0)
		return 0;

	delta = t - s->latency_time;

	/* Server has timed out */
	if (delta > SERVER_TIMEOUT_S) {
		server_disconnect(s, 1, 0, "Ping timeout (" STR(SERVER_TIMEOUT_S) ")");
		return 1;
	}

	/* Server might be timing out, attempt to PING */
	if (delta > SERVER_LATENCY_PING_S && !s->pinging) {
		sendf(NULL, s, "PING :%s", s->host);
		s->pinging = 1;
	}

	/* Server hasn't responded to PING, display latency in status */
	if (delta > SERVER_LATENCY_S && current_channel()->server == s) {
		s->latency_delta = delta;
		draw_status();
	}

	return 0;
}

static int
check_reconnect(struct server *s, time_t t)
{
	/* Check if the server is in auto-reconnect mode, and issue a reconnect if needed */

	if (s->reconnect_time && t > s->reconnect_time) {
		server_connect(s->host, s->port, NULL, NULL);
		return 1;
	}

	return 0;
}

static int
check_socket(struct server *s, time_t t)
{
	/* Check the status of the server's socket */

	ssize_t count;
	char recv_buff[BUFFSIZE];

	/* Consume all input on the socket */
	while (s->soc >= 0 && (count = read(s->soc, recv_buff, BUFFSIZE)) >= 0) {

		if (count == 0) {
			server_disconnect(s, 1, 0, "Remote hangup");
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

	/* Socket is non-blocking, all other errors cause a disconnect */
	if (errno != EWOULDBLOCK && errno != EAGAIN) {
		if (errno)
			server_disconnect(s, 1, 0, strerror(errno));
		else
			server_disconnect(s, 1, 0, "Remote hangup");
	}

	return 0;
}

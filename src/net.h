#ifndef NET_H
#define NET_H

#include "server.h"

#define RECONNECT_DELTA 15

/* TODO: refactoring */
int sendf(char*, struct server*, const char*, ...);
struct server* get_server_head(void);
void check_servers(void);
void server_connect(char*, char*, char*, char*);
void server_disconnect(struct server*, int, int, char*);

#endif

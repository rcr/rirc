#ifndef MESG_H
#define MESG_H

#include <stddef.h>

#include "tree.h"
#include "channel.h"
#include "server.h"

/* Message sent for PART and QUIT by default */
#define DEFAULT_QUIT_MESG "rirc v" VERSION

const struct avl_node* commands_get(const char*, size_t);

void recv_mesg(char*, int, struct server*);
void send_mesg(char*, struct channel*);
void send_paste(char*);

#endif

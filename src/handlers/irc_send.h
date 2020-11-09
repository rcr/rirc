#ifndef RIRC_HANDLERS_IRC_SEND_H
#define RIRC_HANDLERS_IRC_SEND_H

#include "src/components/channel.h"
#include "src/components/server.h"

int irc_send_command(struct server*, struct channel*, char*);
int irc_send_privmsg(struct server*, struct channel*, char*);

#endif

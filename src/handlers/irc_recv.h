#ifndef RIRC_HANDLERS_IRC_RECV_H
#define RIRC_HANDLERS_IRC_RECV_H

#include "src/components/server.h"
#include "src/utils/utils.h"

int irc_recv(struct server*, struct irc_message*);

#endif

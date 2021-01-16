#ifndef RIRC_HANDLERS_IRCV3_H
#define RIRC_HANDLERS_IRCV3_H

#include "src/components/server.h"
#include "src/utils/utils.h"

int ircv3_recv_CAP(struct server*, struct irc_message*);

#endif

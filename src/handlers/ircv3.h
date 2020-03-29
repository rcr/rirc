#ifndef IRCV3_H
#define IRCV3_H

#include "src/components/server.h"
#include "src/utils/utils.h"
#include "src/state.h"

int irc_recv_ircv3(struct server*, struct irc_message*);

#endif

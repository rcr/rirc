#ifndef IRCV3_H
#define IRCV3_H

#include "src/components/server.h"
#include "src/utils/utils.h"

int ircv3_recv_CAP(struct server*, struct irc_message*);

#endif

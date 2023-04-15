#ifndef RIRC_HANDLERS_IRCV3_H
#define RIRC_HANDLERS_IRCV3_H

#include "src/components/server.h"
#include "src/utils/utils.h"

int ircv3_recv_AUTHENTICATE(struct server*, struct irc_message*);
int ircv3_recv_CAP(struct server*, struct irc_message*);

int ircv3_recv_900(struct server*, struct irc_message*);
int ircv3_recv_901(struct server*, struct irc_message*);
int ircv3_recv_902(struct server*, struct irc_message*);
int ircv3_recv_903(struct server*, struct irc_message*);
int ircv3_recv_904(struct server*, struct irc_message*);
int ircv3_recv_905(struct server*, struct irc_message*);
int ircv3_recv_906(struct server*, struct irc_message*);
int ircv3_recv_907(struct server*, struct irc_message*);
int ircv3_recv_908(struct server*, struct irc_message*);

#endif

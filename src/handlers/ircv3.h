#ifndef RIRC_HANDLERS_IRCV3_H
#define RIRC_HANDLERS_IRCV3_H

#include "src/components/server.h"
#include "src/utils/utils.h"

int ircv3_recv_AUTHENTICATE(struct server*, struct irc_message*);
int ircv3_recv_CAP(struct server*, struct irc_message*);

int ircv3_numeric_900(struct server*, struct irc_message*);
int ircv3_numeric_901(struct server*, struct irc_message*);
int ircv3_numeric_902(struct server*, struct irc_message*);
int ircv3_numeric_903(struct server*, struct irc_message*);
int ircv3_numeric_904(struct server*, struct irc_message*);
int ircv3_numeric_905(struct server*, struct irc_message*);
int ircv3_numeric_906(struct server*, struct irc_message*);
int ircv3_numeric_907(struct server*, struct irc_message*);
int ircv3_numeric_908(struct server*, struct irc_message*);

#endif

/* TODO
 *  - cap NEW/DEL
 *  - 0001 -> server registered
 *      - void ircv3_CAP_start(struct server*);
 *      - void ircv3_CAP_end(struct server*);
 *  - cap commands (after registration)
 *      - /cap-list
 *      - /cap-ls
 *      - /cap-req [-]cap [...]
 *  - print LS caps when registered instead of auto caps
 *  - CAP ACK handling for REQS
 *      - CAP ACK when REQ
 *      - CAP -ACK when REQ
 *      - CAP ACK when REQ
 *      - CAP -ACK when -REQ
 *  - cap setting for auto on connect/NEW
 *  - 410 ERR_INVALIDCAPCMD
 *  - handle invalid command on LS during registration
 *    for servers that don't support it
 *  - caps need a current state so they can
 *    return to it on NAK
 *  - replace test cap names with actual cap names
 *  - cap-notify handling
 */

#ifndef IRCV3_H
#define IRCV3_H

enum ircv3_cap
{
	IRCV3_CAP_OFF,
	IRCV3_CAP_ON,
	IRCV3_CAP_PENDING_SEND,
	IRCV3_CAP_PENDING_RECV,
	IRCV3_CAP_ACK,
	IRCV3_CAP_NAK,
};

#include "src/components/server.h"
#include "src/utils/utils.h"
#include "src/state.h"

int ircv3_CAP(struct server*, struct irc_message*);

#endif

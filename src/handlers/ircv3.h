/* TODO
 *  - cap commands (after registration)
 *      - /cap-req [-]cap [...]
 *  - cap setting for auto on connect/NEW
 *      - :set, set/unset auto, and for printing state
 *  - LS caps:
 *      [name] [set/unset] [auto]
 *      [name] [unsupported]
 *  - LIST caps:
 *      [name] [auto]
 *  - CAP args, e.g. cap=foo,bar
 */

#ifndef IRCV3_H
#define IRCV3_H

#include "src/components/server.h"
#include "src/utils/utils.h"

int ircv3_recv_CAP(struct server*, struct irc_message*);

#endif

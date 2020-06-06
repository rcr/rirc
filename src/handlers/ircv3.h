/* TODO
 *  - cap commands (after registration)
 *      - /cap-req [-]cap [...]
 *  - cap NEW/DEL
 *  - cap setting for auto on connect/NEW
 *      - :set, set/unset auto, and for printing state
 *  - 410 ERR_INVALIDCAPCMD
 *  - cap-notify handling
 *  - list caps with `(unsupported)` when printing LS results?
 *  - LS caps:
 *      [name] [set/unset] [auto]
 *      [name] [unsupported]
 *  - LIST caps:
 *      [name] [auto]
 *  - disconnect on CAP protocol errors during registration
 *  - CAP args, e.g. cap=foo,bar
 */

#ifndef IRCV3_H
#define IRCV3_H

#include "src/components/ircv3_cap.h"
#include "src/components/server.h"
#include "src/utils/utils.h"

int ircv3_recv_CAP(struct server*, struct irc_message*);

#endif

// TODO: non ircv3 servers? if the server sends 001, cancel all CAP negotiations
//             Client: CAP LS 302
//             Client: NICK dan
//             Client: USER d * 0 :This is a really good name
//             Server: 001 dan :Welcome to the Internet Relay Network dan
//
// empty CAP list
//      Client: CAP LS
//      Server: CAP * LS :
//
//
// CAP END
//
// 410 (ERR_INVALIDCAPCMD)
//	- and what other ERR types?
//
// cap ls can come in multi-lines... this is a 302 thing
// and so should be handled if sent as 301... see standard
//
//
// what happens if i REQ something that doesn't exist?
//
// client should respond to CAP NEW with CAP REQ if it wants it
//
// cap notify is implicit but might be advertised

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

// TODO
// void ircv3_CAP_start(struct server*);
// void ircv3_CAP_end(struct server*);

#endif

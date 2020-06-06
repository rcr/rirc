#ifndef IRCV3_CAP_H
#define IRCV3_CAP_H

#define IRCV3_CAP_AUTO    1
#define IRCV3_CAP_VERSION "302"

#ifndef IRCV3_CAPS
#define IRCV3_CAPS \
	X("multi-prefix", multi_prefix, IRCV3_CAP_AUTO)
#endif

struct ircv3_cap
{
	unsigned req      : 1; /* cap REQ sent */
	unsigned req_auto : 1; /* cap REQ sent during registration */
	unsigned set      : 1; /* cap is unset/set */
};

struct ircv3_caps
{
	#define X(CAP, VAR, ATTRS) \
	struct ircv3_cap VAR;
	IRCV3_CAPS
	#undef X
	unsigned cap_reqs; /* number of CAP REQs awaiting response */
};

void ircv3_caps(struct ircv3_caps*);
void ircv3_caps_reset(struct ircv3_caps*);

int ircv3_cap_ack(struct ircv3_caps*, const char*);
int ircv3_cap_nak(struct ircv3_caps*, const char*);

const char *ircv3_cap_err(int);

#endif

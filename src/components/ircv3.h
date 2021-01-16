#ifndef RIRC_COMPONENTS_IRCV3_CAP_H
#define RIRC_COMPONENTS_IRCV3_CAP_H

#define IRCV3_CAP_AUTO   (1 << 0)
#define IRCV3_CAP_NO_DEL (1 << 1)
#define IRCV3_CAP_NO_REQ (1 << 2)

#define IRCV3_CAP_VERSION "302"

#define IRCV3_CAPS_DEF \
	X("account-notify", account_notify, IRCV3_CAP_AUTO) \
	X("away-notify",    away_notify,    IRCV3_CAP_AUTO) \
	X("chghost",        chghost,        IRCV3_CAP_AUTO) \
	X("extended-join",  extended_join,  IRCV3_CAP_AUTO) \
	X("invite-notify",  invite_notify,  IRCV3_CAP_AUTO) \
	X("multi-prefix",   multi_prefix,   IRCV3_CAP_AUTO)

/* Extended by testcases */
#ifndef IRCV3_CAPS_TEST
#define IRCV3_CAPS_TEST
#endif

#define IRCV3_CAPS \
	IRCV3_CAPS_DEF \
	IRCV3_CAPS_TEST

struct ircv3_cap
{
	unsigned req          : 1; /* cap REQ sent */
	unsigned req_auto     : 1; /* cap REQ sent during registration */
	unsigned set          : 1; /* cap is unset/set */
	unsigned supported    : 1; /* cap is supported by server */
	unsigned supports_del : 1; /* cap supports CAP DEL */
	unsigned supports_req : 1; /* cap supports CAP REQ */
};

struct ircv3_caps
{
	#define X(CAP, VAR, ATTRS) \
	struct ircv3_cap VAR;
	IRCV3_CAPS
	#undef X
};

struct ircv3_cap* ircv3_cap_get(struct ircv3_caps*, const char*);

void ircv3_caps(struct ircv3_caps*);
void ircv3_caps_reset(struct ircv3_caps*);

#endif

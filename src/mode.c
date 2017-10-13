/* TODO:
 *  - received MODE messages should check the target, either:
 *		- rirc user   (usermode)
 *		- a channel name
 *			- in PREFIX  (chanusermode)
 *			- otherwise  (chanmode)
 *
 *	- safe channels ('!' prefix) (see RFC2811)
 */

#include <string.h>

#include "mode.h"
#include "utils.h"

#define MODE_ISLOWER(X) ((X) >= 'a' && (X) <= 'z')
#define MODE_ISUPPER(X) ((X) >= 'A' && (X) <= 'Z')

/* Set bit Y of X to the value of Z: [0, 1] */
#define MODE_SET(X, Y, Z) ((X) ^= (-(Z) ^ (X)) & (Y))

enum mode_chanmode_prefix_t
{
	MODE_CHANMODE_PREFIX_SECRET  = '@', /* chanmode 's' */
	MODE_CHANMODE_PREFIX_PRIVATE = '*', /* chanmode 'p' */
	MODE_CHANMODE_PREFIX_OTHER   = '=',
	MODE_CHANMODE_PREFIX_T_SIZE
};

static inline int mode_isset(struct mode*, int);
static inline uint32_t flag_bit(int);

static int mode_config_usermodes(struct mode_config*, const char*);
static int mode_config_chanmodes(struct mode_config*, const char*);
static int mode_config_abcd(struct mode_config*, const char*);
static int mode_config_prefix(struct mode_config*, const char*);

static inline int
mode_isset(struct mode *m, int flag)
{
	/* Test if mode flag is set, assumes valid flag */

	if (MODE_ISLOWER(flag) && (m->lower & flag_bit(flag)))
		return 1;

	if (MODE_ISUPPER(flag) && (m->upper & flag_bit(flag)))
		return 1;

	return 0;
}

static inline uint32_t
flag_bit(int c)
{
	/* Map input character to [az-AZ] bit flag */

	/* TODO: consider additional bit as lower/upper case indicator */

	static const uint32_t flag_bits[] = {
		1 << 0,  /* a */ 1 << 1,  /* b */ 1 << 2,  /* c */
		1 << 3,  /* d */ 1 << 4,  /* e */ 1 << 5,  /* f */
		1 << 6,  /* g */ 1 << 7,  /* h */ 1 << 8,  /* i */
		1 << 9,  /* j */ 1 << 10, /* k */ 1 << 11, /* l */
		1 << 12, /* m */ 1 << 13, /* n */ 1 << 14, /* o */
		1 << 15, /* p */ 1 << 16, /* q */ 1 << 17, /* r */
		1 << 18, /* s */ 1 << 19, /* t */ 1 << 20, /* u */
		1 << 21, /* v */ 1 << 22, /* w */ 1 << 23, /* x */
		1 << 24, /* y */ 1 << 25, /* z */
	};

	if (MODE_ISLOWER(c))
		return flag_bits[c - 'a'];

	if (MODE_ISUPPER(c))
		return flag_bits[c - 'A'];

	return 0;
}

int
mode_config(struct mode_config *config, const char *config_str, enum mode_config_t config_t)
{
	/* Initialize a mode_config to the RFC2812, RFC2811 defaults
	 *
	 * Chanmodes (RFC2811, section 4)
	 *
	 *   O - give "channel creator" status;
	 *   o - give/take channel operator privilege;
	 *   v - give/take the voice privilege;
	 *
	 *   a - toggle the anonymous channel flag;
	 *   i - toggle the invite-only channel flag;
	 *   m - toggle the moderated channel;
	 *   n - toggle the no messages to channel from clients on the outside;
	 *   q - toggle the quiet channel flag;
	 *   p - toggle the private channel flag;
	 *   s - toggle the secret channel flag;
	 *   r - toggle the server reop channel flag;
	 *   t - toggle the topic settable by channel operator only flag;
	 *
	 *   k - set/remove the channel key (password);
	 *   l - set/remove the user limit to channel;
	 *
	 *   b - set/remove ban mask to keep users out;
	 *   e - set/remove an exception mask to override a ban mask;
	 *   I - set/remove an invitation mask to automatically override the
	 *       invite-only flag;
	 *
	 * Usermodes (RFC2811, section 3.1.5)
	 *
	 *   a - user is flagged as away;
	 *   i - marks a users as invisible;
	 *   w - user receives wallops;
	 *   r - restricted user connection;
	 *   o - operator flag;
	 *   O - local operator flag;
	 *   s - marks a user for receipt of server notices.
	 *
	 * Note: PREFIX and CHANMODES are ubiquitous additions to the IRC
	 *       protocol given by numeric 005 (RPL_ISUPPORT). As such,
	 *       they've been interpreted here in terms of A,B,C,D subcategories
	 *       for the sake of default settings. Numeric 319 (RPL_WHOISCHANNELS)
	 *       states chanmode user prefixes map o,v to @,+ respectively.
	 */

	switch (config_t) {

		case MODE_CONFIG_DEFAULTS:
			*config = (struct mode_config)
			{
				/* TODO: chanmodes, usermodes, A,B,C,D can be uint32 pairs
				 *       for faster lookups */
				.chanmodes = "OovaimnqpsrtklbeI",
				.usermodes = "aiwroOs",
				.CHANMODES = {
					.A = "beI",
					.B = "k",
					.C = "l",
					.D = "aimnqpsrtO"
				},
				.PREFIX = {
					.F = "ov",
					.T = "@+"
				}
			};
			break;

		case MODE_CONFIG_USERMODES:
			return mode_config_usermodes(config, config_str);

		case MODE_CONFIG_CHANMODES:
			return mode_config_chanmodes(config, config_str);

		case MODE_CONFIG_ABCD:
			return mode_config_abcd(config, config_str);

		case MODE_CONFIG_PREFIX:
			return mode_config_prefix(config, config_str);

		default:
			fatal("mode configuration type unknown", 0);
	}

	return MODE_ERR_NONE;
}

int
mode_chanmode_set(struct mode *m, struct mode_config *config, int flag, enum mode_set_t set_t)
{
	/* Set/unset chanmode flags
	 *
	 * Only CHANMODE subtypes B,C,D set/unset flags for the channel
	 *
	 * ---
	 *
	 * RFC2812, section 5.1, numeric reply 353 (RPL_NAMREPLY)
	 *
	 * "@" is used for secret channels,   ('s' flag)
	 * "*" for private channels, and      ('p' flag)
	 * "=" for others (public channels).
	 *
	 * RFC2811, section 4.2.6 Private and Secret Channels
	 *
	 * The channel flag 'p' is used to mark a channel "private" and the
	 * channel flag 's' to mark a channel "secret".  Both properties are
	 * similar and conceal the existence of the channel from other users.
	 *
	 * This means that there is no way of getting this channel's name from
	 * the server without being a member.  In other words, these channels
	 * MUST be omitted from replies to queries like the WHOIS command.
	 *
	 * When a channel is "secret", in addition to the restriction above, the
	 * server will act as if the channel does not exist for queries like the
	 * TOPIC, LIST, NAMES commands.  Note that there is one exception to
	 * this rule: servers will correctly reply to the MODE command.
	 * Finally, secret channels are not accounted for in the reply to the
	 * LUSERS command (See "Internet Relay Chat: Client Protocol" [IRC-
	 * CLIENT]) when the <mask> parameter is specified.
	 *
	 * The channel flags 'p' and 's' MUST NOT both be set at the same time.
	 * If a MODE message originating from a server sets the flag 'p' and the
	 * flag 's' is already set for the channel, the change is silently
	 * ignored.  This should only happen during a split healing phase
	 * (mentioned in the "IRC Server Protocol" document [IRC-SERVER]).
	 */

	uint32_t bit;

	if (!strchr(config->chanmodes, flag))
		return MODE_ERR_INVALID_FLAG;

	/* Mode subtypes A don't set a flag */
	if (strchr(config->CHANMODES.A, flag))
		return MODE_ERR_NONE;

	if (flag != 's' && flag != 'p') {

		bit = flag_bit(flag);

		if (MODE_ISLOWER(flag))
			MODE_SET(m->lower, bit, set_t);
		else
			MODE_SET(m->upper, bit, set_t);
	}

	else if (flag == 'p') {

		/* Silently ignore */
		if (mode_isset(m, 's'))
			return MODE_ERR_NONE;

		if (set_t == MODE_SET_OFF) {
			MODE_SET(m->lower, flag_bit('p'), MODE_SET_OFF);

			m->prefix = MODE_CHANMODE_PREFIX_OTHER;
		}

		if (set_t == MODE_SET_ON) {
			MODE_SET(m->lower, flag_bit('p'), MODE_SET_ON);

			m->prefix = MODE_CHANMODE_PREFIX_PRIVATE;
		}
	}

	else if (flag == 's') {

		if (set_t == MODE_SET_OFF) {
			MODE_SET(m->lower, flag_bit('s'), MODE_SET_OFF);
			MODE_SET(m->lower, flag_bit('p'), MODE_SET_OFF);

			m->prefix = MODE_CHANMODE_PREFIX_OTHER;
		}

		if (set_t == MODE_SET_ON) {
			MODE_SET(m->lower, flag_bit('s'), MODE_SET_ON);
			MODE_SET(m->lower, flag_bit('p'), MODE_SET_OFF);

			m->prefix = MODE_CHANMODE_PREFIX_SECRET;

		}
	}

	return MODE_ERR_NONE;
}

int
mode_prfxmode_set(struct mode *m, struct mode_config *config, int flag, enum mode_set_t set_t)
{
	/* Set/unset prfxmode flags and mode prefix */

	uint32_t bit;

	if (!strchr(config->PREFIX.F, flag))
		return MODE_ERR_INVALID_FLAG;

	bit = flag_bit(flag);

	if (MODE_ISLOWER(flag))
		MODE_SET(m->lower, bit, set_t);
	else
		MODE_SET(m->upper, bit, set_t);

	char *f = config->PREFIX.F,
	     *t = config->PREFIX.T;

	while (*f) {

		if (mode_isset(m, *f))
			break;

		f++;
		t++;
	}

	m->prefix = *t;

	return MODE_ERR_NONE;
}

int
mode_usermode_set(struct mode *m, struct mode_config *config, int flag, enum mode_set_t set_t)
{
	/* Set/unset usermode flags */

	uint32_t bit;

	if (!strchr(config->usermodes, flag))
		return MODE_ERR_INVALID_FLAG;

	bit = flag_bit(flag);

	if (MODE_ISLOWER(flag))
		MODE_SET(m->lower, bit, set_t);
	else
		MODE_SET(m->upper, bit, set_t);

	return MODE_ERR_NONE;
}

int
mode_chanmode_prefix(struct mode *m, struct mode_config *config, int flag)
{
	/* Set chanmode flag and prefix give the prefix character, e.g.:
	 *
	 * - '@' sets 's', unsets 'p'
	 * - '*' sets 'p'
	 * - '=' sets neither
	 *
	 * All other prefixes are invalid.
	 * Prefixes may override by precendece, but are silentyly ignored otherwise */

	(void)(config);

	/* If 's' is set, all other settings are silently ignored */
	if (m->prefix == MODE_CHANMODE_PREFIX_SECRET)
		return MODE_ERR_NONE;

	/* If 'p' is set, only SECRET prefix is accepted */
	if (m->prefix == MODE_CHANMODE_PREFIX_PRIVATE && flag != MODE_CHANMODE_PREFIX_SECRET)
		return MODE_ERR_NONE;

	/* Otherwise, all valid prefixes can be set */
	switch (flag) {
		case MODE_CHANMODE_PREFIX_SECRET:
			MODE_SET(m->lower, flag_bit('p'), MODE_SET_OFF);
			MODE_SET(m->lower, flag_bit('s'), MODE_SET_ON);
			break;
		case MODE_CHANMODE_PREFIX_PRIVATE:
			MODE_SET(m->lower, flag_bit('p'), MODE_SET_ON);
			break;
		case MODE_CHANMODE_PREFIX_OTHER:
			break;
		default:
			return MODE_ERR_INVALID_PREFIX;
	}

	m->prefix = flag;

	return MODE_ERR_NONE;
}

int
mode_prfxmode_prefix(struct mode *m, struct mode_config *config, int flag)
{
	/* Set prfxmode flag and prefix given the prefix character, e.g.: 
	 *
	 * - if "ov" maps to "@+", then:
	 *   - prfxmode_prefix(config, mode, '@')   sets mode flag 'o'
	 *   - prfxmode_prefix(config, mode, '+')   sets mode flag 'v'
	 */

	uint32_t bit;

	char *f = config->PREFIX.F,
	     *t = config->PREFIX.T;

	while (*t != flag) {

		if (*t == 0)
			return MODE_ERR_INVALID_PREFIX;

		f++;
		t++;
	}

	bit = flag_bit(*f);

	if (MODE_ISLOWER(*f))
		MODE_SET(m->lower, bit, MODE_SET_ON);
	else
		MODE_SET(m->upper, bit, MODE_SET_ON);

	m->prefix = *t;

	return MODE_ERR_NONE;
}

char*
mode_str(struct mode *m, struct mode_str *m_str)
{
	/* Write the mode bits to a mode string */

	char c, *skip = "", *str = m_str->str;

	uint32_t lower = m->lower;
	uint32_t upper = m->upper;

	/* 's' and 'p' flags should not appear in chanmode mode strings */
	switch (m_str->type) {
		case MODE_STR_CHANMODE:
			skip = "sp";
		case MODE_STR_USERMODE:
		case MODE_STR_PRFXMODE:
			break;
		case MODE_STR_UNSET:
			fatal("mode_str type not set", 0);
			break;
		default:
			fatal("mode_str type unknown", 0);
	}

	for (c = 'a'; c <= 'z' && lower; c++, lower >>= 1)
		if ((lower & 1) && !strchr(skip, c))
			*str++ = c;

	for (c = 'A'; c <= 'Z' && upper; c++, upper >>= 1)
		if ((upper & 1) && !strchr(skip, c))
			*str++ = c;

	*str = 0;

	return m_str->str;
}

void
mode_reset(struct mode *m, struct mode_str *s)
{
	/* Set mode and mode_str to initial state */

	if (!m || !s)
		fatal("mode or mode_str is null", 0);

	enum mode_str_t type = s->type;

	memset(m, 0, sizeof(*m));
	memset(s, 0, sizeof(*s));

	s->type = type;
}

static int
mode_config_usermodes(struct mode_config *config, const char *str)
{
	char c, *p = config->usermodes;

	struct mode m = {0};

	uint32_t bit;

	while ((c = *str++)) {

		if ((bit = flag_bit(c)) == 0)
			continue; /* TODO: aggregate warnings, invalid flag */

		if (mode_isset(&m, c))
			continue; /* TODO: aggregate warnings, duplicate flag */

		if (MODE_ISLOWER(c))
			MODE_SET(m.lower, bit, MODE_SET_ON);
		else
			MODE_SET(m.upper, bit, MODE_SET_ON);

		*p++ = c;
	}

	*p = 0;

	return MODE_ERR_NONE;
}

static int
mode_config_chanmodes(struct mode_config *config, const char *str)
{
	char c, *p = config->chanmodes;

	struct mode m = {0};

	uint32_t bit;


	while ((c = *str++)) {

		if ((bit = flag_bit(c)) == 0)
			continue; /* TODO: aggregate warnings, invalid flag */

		if (mode_isset(&m, c))
			continue; /* TODO: aggregate warnings, duplicate flag */

		if (MODE_ISLOWER(c))
			MODE_SET(m.lower, bit, MODE_SET_ON);
		else
			MODE_SET(m.upper, bit, MODE_SET_ON);

		*p++ = c;
	}

	*p = 0;

	return MODE_ERR_NONE;
}

static int
mode_config_abcd(struct mode_config *config, const char *str)
{
	/* TODO: test */

	(void)(config);
	(void)(str);
	return MODE_ERR_NONE;
}

static int
mode_config_prefix(struct mode_config *config, const char *str)
{
	/* TODO: test */

	(void)(config);
	(void)(str);
	return MODE_ERR_NONE;
}

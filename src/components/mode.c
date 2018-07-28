/* TODO: safe channels ('!' prefix) (see RFC2811) */

#include <ctype.h>
#include <string.h>

#include "src/components/mode.h"
#include "src/utils/utils.h"

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

static inline int mode_isset(const struct mode*, int);
static inline uint32_t flag_bit(int);

static enum mode_err_t mode_config_chanmodes(struct mode_config*, const char*);
static enum mode_err_t mode_config_usermodes(struct mode_config*, const char*);
static enum mode_err_t mode_config_subtypes(struct mode_config*, const char*);
static enum mode_err_t mode_config_prefix(struct mode_config*, const char*);
static enum mode_err_t mode_config_modes(struct mode_config*, const char*);

/* TODO: check validity of set_t on all mode settings */
/* TODO: static inline void mode_bit_set(struct mode*, uint32_t); */
/* TODO: static inline void mode_bit_isset(struct mode*, uint32_t); */
/* TODO: aggregate errors with logging callback */

static inline int
mode_isset(const struct mode *m, int flag)
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

	static const uint32_t flag_bits[] = {
		1U << 0,  /* a */ 1U << 1,  /* b */ 1U << 2,  /* c */
		1U << 3,  /* d */ 1U << 4,  /* e */ 1U << 5,  /* f */
		1U << 6,  /* g */ 1U << 7,  /* h */ 1U << 8,  /* i */
		1U << 9,  /* j */ 1U << 10, /* k */ 1U << 11, /* l */
		1U << 12, /* m */ 1U << 13, /* n */ 1U << 14, /* o */
		1U << 15, /* p */ 1U << 16, /* q */ 1U << 17, /* r */
		1U << 18, /* s */ 1U << 19, /* t */ 1U << 20, /* u */
		1U << 21, /* v */ 1U << 22, /* w */ 1U << 23, /* x */
		1U << 24, /* y */ 1U << 25, /* z */
	};

	if (MODE_ISLOWER(c))
		return flag_bits[c - 'a'];

	if (MODE_ISUPPER(c))
		return flag_bits[c - 'A'];

	return 0;
}

enum mode_err_t
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
	 * MODES (RFC2811, section 3.2.3)
	 *
	 *   "Note that there is a maximum limit of three (3) changes per command
	 *    for modes that take a parameter."
	 *
	 * Note: PREFIX, MODES and CHANMODES are ubiquitous additions to the IRC
	 *       protocol given by numeric 005 (RPL_ISUPPORT). As such,
	 *       they've been interpreted here in terms of A,B,C,D subcategories
	 *       for the sake of default settings. Numeric 319 (RPL_WHOISCHANNELS)
	 *       states chanmode user prefixes map o,v to @,+ respectively.
	 */

	switch (config_t) {

		case MODE_CONFIG_DEFAULTS:
			*config = (struct mode_config)
			{
				.PREFIX = {
					.F = "ov",
					.T = "@+"
				},
				.MODES = 3
			};
			mode_config_chanmodes(config, "OovaimnqpsrtklbeI");
			mode_config_usermodes(config, "aiwroOs");
			mode_config_subtypes(config, "beI,k,l,aimnqpsrtO");
			break;

		case MODE_CONFIG_CHANMODES:
			return mode_config_chanmodes(config, config_str);

		case MODE_CONFIG_USERMODES:
			return mode_config_usermodes(config, config_str);

		case MODE_CONFIG_SUBTYPES:
			return mode_config_subtypes(config, config_str);

		case MODE_CONFIG_PREFIX:
			return mode_config_prefix(config, config_str);

		case MODE_CONFIG_MODES:
			return mode_config_modes(config, config_str);

		default:
			fatal("mode configuration type unknown", 0);
	}

	return MODE_ERR_NONE;
}

enum mode_err_t
mode_chanmode_set(struct mode *m, const struct mode_config *config, int flag, enum mode_set_t set_t)
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

	if (!(set_t == MODE_SET_ON || set_t == MODE_SET_OFF))
		return MODE_ERR_INVALID_SET;

	if (!mode_isset(&(config->chanmodes), flag))
		return MODE_ERR_INVALID_FLAG;

	/* Mode subtypes A don't set a flag */
	if (mode_isset(&(config->CHANMODES.A), flag))
		return MODE_ERR_NONE;

	if (flag != 's' && flag != 'p') {

		uint32_t bit = flag_bit(flag);

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

enum mode_err_t
mode_prfxmode_set(struct mode *m, const struct mode_config *config, int flag, enum mode_set_t set_t)
{
	/* Set/unset prfxmode flags and mode prefix */

	uint32_t bit;

	if (!(set_t == MODE_SET_ON || set_t == MODE_SET_OFF))
		return MODE_ERR_INVALID_SET;

	if (!strchr(config->PREFIX.F, flag))
		return MODE_ERR_INVALID_FLAG;

	bit = flag_bit(flag);

	if (MODE_ISLOWER(flag))
		MODE_SET(m->lower, bit, set_t);
	else
		MODE_SET(m->upper, bit, set_t);

	const char *f = config->PREFIX.F,
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

enum mode_err_t
mode_usermode_set(struct mode *m, const struct mode_config *config, int flag, enum mode_set_t set_t)
{
	/* Set/unset usermode flags */

	uint32_t bit;

	if (!(set_t == MODE_SET_ON || set_t == MODE_SET_OFF))
		return MODE_ERR_INVALID_SET;

	if (!mode_isset(&(config->usermodes), flag))
		return MODE_ERR_INVALID_FLAG;

	bit = flag_bit(flag);

	if (MODE_ISLOWER(flag))
		MODE_SET(m->lower, bit, set_t);
	else
		MODE_SET(m->upper, bit, set_t);

	return MODE_ERR_NONE;
}

enum mode_err_t
mode_chanmode_prefix(struct mode *m, const struct mode_config *config, int flag)
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

enum mode_err_t
mode_prfxmode_prefix(struct mode *m, const struct mode_config *config, int flag)
{
	/* Set prfxmode flag and prefix given the prefix character, e.g.: 
	 *
	 * - if "ov" maps to "@+", then:
	 *   - prfxmode_prefix(config, mode, '@')   sets mode flag 'o'
	 *   - prfxmode_prefix(config, mode, '+')   sets mode flag 'v'
	 */

	uint32_t bit;

	const char *f = config->PREFIX.F,
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
mode_str(const struct mode *m, struct mode_str *m_str)
{
	/* Write the mode bits to a mode string */

	char c, *skip = "", *str = m_str->str;

	uint32_t lower = m->lower,
	         upper = m->upper;

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

static enum mode_err_t
mode_config_chanmodes(struct mode_config *config, const char *str)
{
	/* Parse and configure chanmodes string */

	char c;
	struct mode *chanmodes = &(config->chanmodes);

	chanmodes->lower = 0;
	chanmodes->upper = 0;

	while ((c = *str++)) {

		uint32_t bit;

		if ((bit = flag_bit(c)) == 0)
			continue; /* TODO: aggregate warnings, invalid flag */

		if (mode_isset(chanmodes, c))
			continue; /* TODO: aggregate warnings, duplicate flag */

		if (MODE_ISLOWER(c))
			MODE_SET(chanmodes->lower, bit, MODE_SET_ON);
		else
			MODE_SET(chanmodes->upper, bit, MODE_SET_ON);
	}

	return MODE_ERR_NONE;
}

static enum mode_err_t
mode_config_usermodes(struct mode_config *config, const char *str)
{
	/* Parse and configure usermodes string */

	char c;
	struct mode *usermodes = &(config->usermodes);

	usermodes->lower = 0;
	usermodes->upper = 0;

	while ((c = *str++)) {

		uint32_t bit;

		if ((bit = flag_bit(c)) == 0)
			continue; /* TODO: aggregate warnings, invalid flag */

		if (mode_isset(usermodes, c))
			continue; /* TODO: aggregate warnings, duplicate flag */

		if (MODE_ISLOWER(c))
			MODE_SET(usermodes->lower, bit, MODE_SET_ON);
		else
			MODE_SET(usermodes->upper, bit, MODE_SET_ON);
	}

	return MODE_ERR_NONE;
}

static enum mode_err_t
mode_config_subtypes(struct mode_config *config, const char *str)
{
	/* Parse and configure CHANMODE subtypes, e.g.:
	 *
	 * "abc,d,ef,xyz" sets mode bits:
	 *  - A = a | b | c
	 *  - B = d
	 *  - C = e | f
	 *  - D = x | y | z
	 */

	char c;

	struct mode *subtypes[] = {
		&(config->CHANMODES.A),
		&(config->CHANMODES.B),
		&(config->CHANMODES.C),
		&(config->CHANMODES.D)
	};

	struct mode duplicates, *setting = subtypes[0];

	memset(&(config->CHANMODES.A), 0, sizeof (struct mode));
	memset(&(config->CHANMODES.B), 0, sizeof (struct mode));
	memset(&(config->CHANMODES.C), 0, sizeof (struct mode));
	memset(&(config->CHANMODES.D), 0, sizeof (struct mode));
	memset(&duplicates, 0, sizeof (struct mode));

	uint32_t commas = 0;

	while ((c = *str++)) {

		if (c == ',') {
			switch (commas) {
				case 0:
				case 1:
				case 2:
					setting = subtypes[++commas];
					break;
				default:
					return MODE_ERR_INVALID_CONFIG;
			}
			continue;
		}

		uint32_t bit;

		if ((bit = flag_bit(c)) == 0)
			continue; /* TODO: aggregate warnings, invalid flag */

		if (mode_isset(&duplicates, c))
			continue; /* TODO: aggregate warnings, duplicate flag */

		if (MODE_ISLOWER(c)) {
			MODE_SET(duplicates.lower, bit, MODE_SET_ON);
			MODE_SET(setting->lower, bit, MODE_SET_ON);
		}
		else {
			MODE_SET(duplicates.upper, bit, MODE_SET_ON);
			MODE_SET(setting->upper, bit, MODE_SET_ON);
		}
	}

	return MODE_ERR_NONE;
}

static enum mode_err_t
mode_config_prefix(struct mode_config *config, const char *str)
{
	/* Parse and configure PREFIX e.g.:
	 *
	 * "(abc)!@#" maps
	 *  - a -> !
	 *  - b -> @
	 *  - c -> #
	 */

	char *str_f, cf,
	     *str_t, ct,
	     *config_f = config->PREFIX.F,
	     *config_t = config->PREFIX.T,
	     _str[strlen(str) + 1];

	struct mode duplicates;

	memcpy(_str, str, sizeof(_str));

	if (*(str_f = _str) != '(')
		goto error;

	if (!(str_t = strchr(str_f, ')')))
		goto error;

	*str_f++ = 0;
	*str_t++ = 0;

	if (strlen(str_f) != strlen(str_t))
		goto error;

	memset(&duplicates, 0, sizeof duplicates);

	while (*str_f) {

		uint32_t bit;

		cf = *str_f++;
		ct = *str_t++;

		/* Check printable prefix */
		if (!(isgraph(ct)))
			goto error;

		/* Check valid flag */
		if ((bit = flag_bit(cf)) == 0)
			goto error;

		/* Check duplicates */
		if (mode_isset(&duplicates, cf))
			goto error;

		if (MODE_ISLOWER(cf))
			MODE_SET(duplicates.lower, bit, MODE_SET_ON);
		else
			MODE_SET(duplicates.upper, bit, MODE_SET_ON);

		*config_f++ = cf;
		*config_t++ = ct;
	}

	*config_f = 0;
	*config_t = 0;

	return MODE_ERR_NONE;

error:

	*(config->PREFIX.F) = 0;
	*(config->PREFIX.T) = 0;

	return MODE_ERR_INVALID_CONFIG;
}

static enum mode_err_t
mode_config_modes(struct mode_config *config, const char *str)
{
	/* Parse and configure MODES, valid values are numeric strings [1-99] */

	unsigned int modes = 0;

	for (; modes < 100 && *str; str++) {
		if (isdigit(*str))
			modes = modes * 10 + (*str - '0');
		else
			return MODE_ERR_INVALID_CONFIG;
	}

	if (!(modes > 0 && modes < 100))
		return MODE_ERR_INVALID_CONFIG;

	config->MODES = modes;

	return MODE_ERR_NONE;
}

enum chanmode_flag_t
chanmode_type(const struct mode_config *config, enum mode_set_t set, int flag)
{
	/* Return the chanmode flag type specified by config */

	if (!(set == MODE_SET_ON || set == MODE_SET_OFF))
		return MODE_FLAG_INVALID_SET;

	if (mode_isset(&(config->chanmodes), flag)) {

		if (strchr(config->PREFIX.F, flag))
			return MODE_FLAG_PREFIX;

		if (mode_isset(&(config->CHANMODES.A), flag))
			return MODE_FLAG_CHANMODE_PARAM;

		if (mode_isset(&(config->CHANMODES.B), flag))
			return MODE_FLAG_CHANMODE_PARAM;

		if (mode_isset(&(config->CHANMODES.C), flag)) {

			if (set == MODE_SET_ON)
				return MODE_FLAG_CHANMODE_PARAM;

			if (set == MODE_SET_OFF)
				return MODE_FLAG_CHANMODE;
		}

		if (mode_isset(&(config->CHANMODES.D), flag))
			return MODE_FLAG_CHANMODE;
	}

	return MODE_FLAG_INVALID_FLAG;
}

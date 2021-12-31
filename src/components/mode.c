#include "src/components/mode.h"

#include "src/utils/utils.h"

#include <ctype.h>
#include <string.h>

/* Set bit Y of X to the value of Z: [0, 1] */
#define MODE_SET_BIT(X, Y, Z) ((X) ^= (-(Z) ^ (X)) & (Y))

static int mode_isset(const struct mode*, int);
static void mode_set(struct mode*, int, int);
static uint32_t mode_bit(uint8_t);

static int mode_cfg_chanmodes(struct mode_cfg*, const char*);
static int mode_cfg_usermodes(struct mode_cfg*, const char*);
static int mode_cfg_subtypes(struct mode_cfg*, const char*);
static int mode_cfg_prefix(struct mode_cfg*, const char*);

static uint32_t
mode_bit(uint8_t c)
{
	static const uint32_t mode_bits[] = {
		['a'] = (1U << 0), ['j'] = (1U << 9),  ['s'] = (1U << 18),
		['b'] = (1U << 1), ['k'] = (1U << 10), ['t'] = (1U << 19),
		['c'] = (1U << 2), ['l'] = (1U << 11), ['u'] = (1U << 20),
		['d'] = (1U << 3), ['m'] = (1U << 12), ['v'] = (1U << 21),
		['e'] = (1U << 4), ['n'] = (1U << 13), ['w'] = (1U << 22),
		['f'] = (1U << 5), ['o'] = (1U << 14), ['x'] = (1U << 23),
		['g'] = (1U << 6), ['p'] = (1U << 15), ['y'] = (1U << 24),
		['h'] = (1U << 7), ['q'] = (1U << 16), ['z'] = (1U << 25),
		['i'] = (1U << 8), ['r'] = (1U << 17), [UINT8_MAX] = 0
	};

	return mode_bits[tolower(c)];
}

static void
mode_set(struct mode *m, int flag, int set)
{
	if (islower(flag))
		MODE_SET_BIT(m->lower, mode_bit(flag), !!set);

	if (isupper(flag))
		MODE_SET_BIT(m->upper, mode_bit(flag), !!set);
}

static int
mode_isset(const struct mode *m, int flag)
{
	if (islower(flag) && (m->lower & mode_bit(flag)))
		return 1;

	if (isupper(flag) && (m->upper & mode_bit(flag)))
		return 1;

	return 0;
}

int
mode_cfg(struct mode_cfg *cfg, const char *cfg_str, enum mode_cfg_type cfg_type)
{
	/* Initialize a mode_cfg to the RFC2812, RFC2811 defaults
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
	 *   I - set/remove an invitation mask to automatically override the invite-only flag;
	 *
	 * Usermodes (RFC2812, section 3.1.5)
	 *
	 *   a - user is flagged as away;
	 *   i - marks a users as invisible;
	 *   w - user receives wallops;
	 *   r - restricted user connection;
	 *   o - operator flag;
	 *   O - local operator flag;
	 *   s - marks a user for receipt of server notices.
	 */

	switch (cfg_type) {

		case MODE_CFG_DEFAULTS:
			memset(cfg, 0, sizeof(*cfg));
			(void) snprintf(cfg->PREFIX.F, sizeof(cfg->PREFIX.F), "ov");
			(void) snprintf(cfg->PREFIX.T, sizeof(cfg->PREFIX.T), "@+");
			mode_cfg_chanmodes(cfg, "OovaimnqpsrtklbeI");
			mode_cfg_usermodes(cfg, "aiwroOs");
			mode_cfg_subtypes(cfg, "IObe,k,l,aimnqpsrt");
			break;

		case MODE_CFG_CHANMODES:
			return mode_cfg_chanmodes(cfg, cfg_str);

		case MODE_CFG_USERMODES:
			return mode_cfg_usermodes(cfg, cfg_str);

		case MODE_CFG_SUBTYPES:
			return mode_cfg_subtypes(cfg, cfg_str);

		case MODE_CFG_PREFIX:
			return mode_cfg_prefix(cfg, cfg_str);

		default:
			fatal("mode configuration type unknown: %d", cfg_type);
	}

	return 0;
}

int
mode_chanmode_set(struct mode *m, const struct mode_cfg *cfg, int flag, int set)
{
	/* Set/unset chanmode flags */

	if (!mode_isset(&(cfg->chanmodes), flag))
		return -1;

	if (mode_isset(&(cfg->CHANMODES.A), flag))
		return 0;

	mode_set(m, flag, set);

	return 0;
}

int
mode_prfxmode_set(struct mode *m, const struct mode_cfg *cfg, int flag, int set)
{
	/* Set/unset prfxmode flag or prefix */

	const char *f = cfg->PREFIX.F;
	const char *t = cfg->PREFIX.T;

	while (*f && *t && *f != flag && *t != flag) {
		f++;
		t++;
	}

	if (!*f || !*t)
		return -1;

	mode_set(m, *f, set);

	f = cfg->PREFIX.F;
	t = cfg->PREFIX.T;

	while (*f) {

		if (mode_isset(m, *f))
			break;

		f++;
		t++;
	}

	m->prefix = *t;

	return 0;
}

int
mode_usermode_set(struct mode *m, const struct mode_cfg *cfg, int flag, int set)
{
	/* Set/unset usermode flags */

	if (!mode_isset(&(cfg->usermodes), flag))
		return -1;

	mode_set(m, flag, set);

	return 0;
}

const char*
mode_str(const struct mode *m, struct mode_str *m_str)
{
	/* Write the mode bits to a mode string */

	char *str = m_str->str;

	uint32_t lower = m->lower;
	uint32_t upper = m->upper;

	for (char c = 'a'; c <= 'z' && lower; c++, lower >>= 1)
		if (lower & 1)
			*str++ = c;

	for (char c = 'A'; c <= 'Z' && upper; c++, upper >>= 1)
		if (upper & 1)
			*str++ = c;

	*str = 0;

	return m_str->str;
}

static int
mode_cfg_chanmodes(struct mode_cfg *cfg, const char *str)
{
	/* Parse and configure chanmodes string */

	char c;
	struct mode *chanmodes = &(cfg->chanmodes);

	chanmodes->lower = 0;
	chanmodes->upper = 0;

	while ((c = *str++)) {

		if (!mode_bit(c))
			continue;

		if (mode_isset(chanmodes, c))
			continue;

		mode_set(chanmodes, c, 1);
	}

	return 0;
}

static int
mode_cfg_usermodes(struct mode_cfg *cfg, const char *str)
{
	/* Parse and configure usermodes string */

	char c;
	struct mode *usermodes = &(cfg->usermodes);

	usermodes->lower = 0;
	usermodes->upper = 0;

	while ((c = *str++)) {

		if (!mode_bit(c))
			continue;

		if (mode_isset(usermodes, c))
			continue;

		mode_set(usermodes, c, 1);
	}

	return 0;
}

static int
mode_cfg_subtypes(struct mode_cfg *cfg, const char *str)
{
	/* Parse and configure CHANMODE subtypes, e.g.:
	 *
	 * "abc,d,ef,xyz" sets mode bits:
	 *  - A = abc
	 *  - B = d
	 *  - C = ef
	 *  - D = xyz
	 */

	char c;

	struct mode *subtypes[] = {
		&(cfg->CHANMODES.A),
		&(cfg->CHANMODES.B),
		&(cfg->CHANMODES.C),
		&(cfg->CHANMODES.D)
	};

	struct mode *setting = subtypes[0];

	memset(&(cfg->CHANMODES.A), 0, sizeof(struct mode));
	memset(&(cfg->CHANMODES.B), 0, sizeof(struct mode));
	memset(&(cfg->CHANMODES.C), 0, sizeof(struct mode));
	memset(&(cfg->CHANMODES.D), 0, sizeof(struct mode));

	unsigned commas = 0;

	while ((c = *str++)) {

		if (c == ',') {
			switch (commas) {
				case 0:
				case 1:
				case 2:
					setting = subtypes[++commas];
					continue;
				default:
					goto error;
			}
		}

		if (!mode_bit(c))
			goto error;

		mode_set(setting, c, 1);
	}

	return 0;

error:

	memset(&(cfg->CHANMODES.A), 0, sizeof(struct mode));
	memset(&(cfg->CHANMODES.B), 0, sizeof(struct mode));
	memset(&(cfg->CHANMODES.C), 0, sizeof(struct mode));
	memset(&(cfg->CHANMODES.D), 0, sizeof(struct mode));

	return -1;
}

static int
mode_cfg_prefix(struct mode_cfg *cfg, const char *str)
{
	/* Parse and configure PREFIX e.g.:
	 *
	 * "(abc)!@#" maps
	 *  - a -> !
	 *  - b -> @
	 *  - c -> #
	 */

	char *cfg_f = cfg->PREFIX.F;
	char *cfg_t = cfg->PREFIX.T;
	char *dup = irc_strdup(str);
	char *str_f;
	char *str_t;

	memset(cfg->PREFIX.F, 0, sizeof(cfg->PREFIX.F));
	memset(cfg->PREFIX.T, 0, sizeof(cfg->PREFIX.T));

	if (*(str_f = dup) != '(')
		goto error;

	if (!(str_t = strchr(str_f, ')')))
		goto error;

	*str_f++ = 0;
	*str_t++ = 0;

	if (strlen(str_f) != strlen(str_t))
		goto error;

	while (*str_f) {

		char cf = *str_f++;
		char ct = *str_t++;

		if (!mode_bit(cf))
			goto error;

		if (!(isgraph(ct)))
			goto error;

		if (strchr(cfg->PREFIX.F, cf))
			goto error;

		*cfg_f++ = cf;
		*cfg_t++ = ct;
	}

	*cfg_f = 0;
	*cfg_t = 0;

	free(dup);

	return 0;

error:

	*(cfg->PREFIX.F) = 0;
	*(cfg->PREFIX.T) = 0;

	free(dup);

	return -1;
}

enum mode_type
mode_type(const struct mode_cfg *cfg, int flag, int set)
{
	/* Chanmode PREFIX */
	if (strchr(cfg->PREFIX.F, flag))
		return MODE_FLAG_PREFIX;

	/* Chanmode subtype A, Always has a parameter. */
	if (mode_isset(&(cfg->CHANMODES.A), flag))
		return MODE_FLAG_CHANMODE_PARAM;

	/* Chanmode subtype B, Always has a parameter. */
	if (mode_isset(&(cfg->CHANMODES.B), flag))
		return MODE_FLAG_CHANMODE_PARAM;

	/* Chanmode subtype C, Only has a parameter when set. */
	if (mode_isset(&(cfg->CHANMODES.C), flag))
		return (set ? MODE_FLAG_CHANMODE_PARAM : MODE_FLAG_CHANMODE);

	/* Chanmode subtype D, Never has a parameter. */
	if (mode_isset(&(cfg->CHANMODES.D), flag))
		return MODE_FLAG_CHANMODE;

	return MODE_FLAG_INVALID_FLAG;
}

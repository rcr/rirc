#ifndef MODE_H
#define MODE_H

/* usermodes, chanmodes and prfxmode configuration
 *
 * `usermodes`, `chanmodes`, parsed from numeric 004 (RPL_MYINFO)
 * `CHANMODES`, `PREFIX`,    parsed from numeric 005 (RPL_ISUPPORT)
 *
 * Three categories of modes exist, depending on the MODE message target:
 *   - Modes set server-wide for the rirc user (usermode)
 *   - Modes set for a channel                 (chanmode)
 *   - Modes set for a user on a channel       (prfxmode)
 *
 * mode_config.chanmodes apply to a channel and the subtypes are given by A,B,C,D:
 *   - A = Mode that adds or removes a nick or address to a list. Always has a parameter.
 *   - B = Mode that changes a setting and always has a parameter.
 *   - C = Mode that changes a setting and only has a parameter when set.
 *   - D = Mode that changes a setting and never has a parameter.
 *
 * mode_config.usermodes apply to the IRC user at a server level
 *
 * PREFIX maps a subset of modes to user prefixes for that channel, in order of
 * precedence. Multiple prefix modes can be set for a user, but only one mode flag
 * should be shown, e.g.:
 *   - if "ov" maps to "@+", then:
 *     - user +v  ->  "+user"
 *     - user +o  ->  "@user"
 *     - user -o  ->  "+user"
 *     - user -v  ->   "user"
 *
 * PREFIX modes are not included in CHANMODES
 *
 * MODES specifies the maximum number of channel modes with parameter
 * allowed per MODE command
 *
 * Numeric 353 (RPL_NAMREPLY) sets chanmode and prfxmode for users on a channel
 * by providing the prefix character rather than the flag
 */

#include <stdint.h>

/* [azAZ] */
#define MODE_STR_LEN 26 * 2

#define MODE_EMPTY (struct mode) \
{                \
    .prefix = 0, \
    .lower  = 0, \
    .upper  = 0, \
}

enum mode_err_t
{
	MODE_ERR_INVALID_CONFIG = -4,
	MODE_ERR_INVALID_PREFIX = -3,
	MODE_ERR_INVALID_FLAG   = -2,
	MODE_ERR_INVALID_SET    = -1,
	MODE_ERR_NONE
};

enum mode_config_t
{
	MODE_CONFIG_DEFAULTS,  /* Set RFC2811 mode defaults */
	MODE_CONFIG_CHANMODES, /* Set numeric 004 chanmdoes string */
	MODE_CONFIG_USERMODES, /* Set numeric 004 usermodes string */
	MODE_CONFIG_PREFIX,    /* Set numeric 005 PREFIX */
	MODE_CONFIG_SUBTYPES,  /* Set numeric 005 CHANMODES subtypes */
	MODE_CONFIG_MODES,     /* Set numeric 005 MODES */
	MODE_CONFIG_T_SIZE
};

enum mode_set_t
{
	MODE_SET_OFF = 0,
	MODE_SET_ON  = 1,
	MODE_SET_INVALID,
	MODE_SET_T_SIZE
};

enum chanmode_flag_t
{
	MODE_FLAG_INVALID_FLAG,
	MODE_FLAG_INVALID_SET,
	MODE_FLAG_CHANMODE,       /* Chanmode flag without parameter */
	MODE_FLAG_CHANMODE_PARAM, /* Chanmode flag with parameter */
	MODE_FLAG_PREFIX,         /* Chanmode flag that sets prfxmode */
	MODE_FLAG_T_SIZE
};

struct mode
{
	char prefix;    /* Prefix character for chanmode, prfxmode */
	uint32_t lower; /* Lowercase mode bits */
	uint32_t upper; /* Uppercase mode bits */
};

struct mode_config
{
	unsigned int MODES;    /* Numeric 005 MODES */
	struct mode chanmodes; /* Numeric 004 chanmodes string */
	struct mode usermodes; /* Numeric 004 usermodes string */
	struct
	{
		struct mode A; /* Numeric 005 CHANMODES substrings */
		struct mode B;
		struct mode C;
		struct mode D;
	} CHANMODES;
	struct
	{
		char F[MODE_STR_LEN + 1]; /* prfxmode mapping `from` */
		char T[MODE_STR_LEN + 1]; /* prfxmode mapping `to`  */
	} PREFIX;
};

struct mode_str
{
	char str[MODE_STR_LEN + 1];
	enum mode_str_t
	{
		MODE_STR_UNSET = 0, /* Ensure a mode_str type is explicitly set */
		MODE_STR_CHANMODE,
		MODE_STR_USERMODE,
		MODE_STR_PRFXMODE,
		MODE_STR_T_SIZE
	} type;
};

enum chanmode_flag_t chanmode_type(const struct mode_config*, enum mode_set_t, int);

enum mode_err_t mode_config(struct mode_config*, const char*, enum mode_config_t);

enum mode_err_t mode_chanmode_set(struct mode*, const struct mode_config*, int, enum mode_set_t);
enum mode_err_t mode_prfxmode_set(struct mode*, const struct mode_config*, int, enum mode_set_t);
enum mode_err_t mode_usermode_set(struct mode*, const struct mode_config*, int, enum mode_set_t);

enum mode_err_t mode_chanmode_prefix(struct mode*, const struct mode_config*, int);
enum mode_err_t mode_prfxmode_prefix(struct mode*, const struct mode_config*, int);

char* mode_str(const struct mode*, struct mode_str*);

void mode_reset(struct mode*, struct mode_str*);

#endif

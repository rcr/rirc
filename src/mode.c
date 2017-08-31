#include <ctype.h>

#include "mode.h"

/* TODO:
 *  - channel mode (secret, private, etc) flags can be toggled,
 *  - p can never be set when s is set in private/secret case, if this message
 *    is received it is silently ignored (RFC2811 section 4.2.6)
 *
 *  - received MODE messages should check the target, either:
 *		- rirc user   (usermode)
 *		- a channel name
 *			- in PREFIX  (chanusermode)
 *			- otherwise  (chanmode)
 *
 *	- safe channels ('!' prefix) (see RFC2811)
 */

static inline char char_mode(char);
static inline char mode_char(char);

static char mode_get_prefix(struct mode_config*, char, char);

static void mode_set_str(char*, char*);

void
mode_config_defaults(struct mode_config *m)
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
	 * Usermodes (RFC2118, section 3.1.5)
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

	*m = (struct mode_config)
	{
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
}

static inline char
char_mode(char c)
{
	if (isupper(c))
		return c - 'A' + 26;

	if (islower(c))
		return c - 'a';

	return -1;
}

static inline char
mode_char(char m)
{
	return (m < 26 ? m + 'a' : m + 'A' - 26);
}

static char
mode_get_prefix(struct mode_config *m, char prefix, char mode)
{
	/* Return the most prescedent user prefix, given a current prefix
	 * and a new mode flag */

	int from = 0, to = 0;

	char *prefix_f = m->PREFIX.F,
	     *prefix_t = m->PREFIX.T;

	while (*prefix_f && *prefix_f++ != mode)
		from++;

	while (*prefix_t && *prefix_t++ != prefix)
		to++;

	return (from < to) ? m->PREFIX.T[from] : prefix;
}

static void
mode_set_str(char *modes, char *str)
{
	/* Repack the mode string with set modes */

	int i;

	for (i = 0; i <= MODE_LEN; i++)
		if (modes[i])
			*str++ = mode_char(i);

	*str = 0;
}

int
usermode_set(struct usermode *usermode, int mode)
{
	if ((mode = char_mode(mode)) < 0)
		return 1;

	usermode->modes[mode] = 1;

	mode_set_str(usermode->modes, usermode->str);

	return 0;
}

int
usermode_unset(struct usermode *usermode, int mode)
{
	if ((mode = char_mode(mode)) < 0)
		return 1;

	usermode->modes[mode] = 0;

	mode_set_str(usermode->modes, usermode->str);

	return 0;
}

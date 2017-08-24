/* Usermodes, Chanmodes and server mode configuration
 *
 * `usermodes`, `chanmodes`, parsed from numeric 004 (RPL_MYINFO)
 * `CHANMODES`, `PREFIX`, parsed from numeric 005 (RPL_ISUPPORT)
 *
 * Chanmodes apply to a channel and the subtypes are given by A,B,C,D:
 *   - A = Mode that adds or removes a nick or address to a list. Always has a parameter.
 *   - B = Mode that changes a setting and always has a parameter.
 *   - C = Mode that changes a setting and only has a parameter when set.
 *   - D = Mode that changes a setting and never has a parameter.
 *
 * Usermodes apply to the IRC user at a server level
 *
 * Prefix maps a subset of chanmodes to user prefixes for that channel, in order of
 * precedence. Multiple prefix modes can be set for a user, but only one mode flag
 * should be shown, e.g.:
 *   - if "ov" maps to "@+", then:
 *     - user +v  ->  "+user"
 *     - user +o  ->  "@user"
 *     - user -o  ->  "+user"
 *     - user -v  ->   "user"
 */

/* [a-zA-Z] */
#define MODE_LEN 26 * 2

struct mode_config
{
	char chanmodes[MODE_LEN + 1];
	char usermodes[MODE_LEN + 1];
	struct {
		char *A;
		char *B;
		char *C;
		char *D;
		char _[MODE_LEN + 4];
	} CHANMODES;
	struct {
		char F[MODE_LEN + 1];
		char T[MODE_LEN + 1];
	} PREFIX;
};

struct mode
{
	char str[MODE_LEN + 1];
};

void mode_defaults(struct mode_config*);

char mode_get_prefix(struct mode_config*, char, char);

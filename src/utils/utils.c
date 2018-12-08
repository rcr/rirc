#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "src/utils/utils.h"

static inline int irc_toupper(int);

char*
getarg(char **str, const char *sep)
{
	/* Return a token parsed from *str delimited by any character in sep.
	 *
	 * Consumes all sep characters preceding the token and null terminates it.
	 *
	 * Returns NULL if *str is NULL or contains only sep characters */

	char *ret, *ptr;

	if (str == NULL || (ptr = *str) == NULL)
		return NULL;

	while (*ptr && strchr(sep, *ptr))
		ptr++;

	if (*ptr == '\0')
		return NULL;

	ret = ptr;

	while (*ptr && !strchr(sep, *ptr))
		ptr++;

	/* If the string continues after the found arg, set the input to point
	 * one past the arg's null terminator.
	 *
	 * This might result in *str pointing to the original string's null
	 * terminator, in which case the next call to getarg will return NULL */

	*str = ptr + (*ptr && strchr(sep, *ptr));

	*ptr = '\0';

	return ret;
}

char*
strdup(const char *str)
{
	return memdup(str, strlen(str) + 1);
}

void*
memdup(const void *mem, size_t len)
{
	void *ret;

	if ((ret = malloc(len)) == NULL)
		fatal("malloc: %s", strerror(errno));

	memcpy(ret, mem, len);

	return ret;
}

int
str_trim(char **str)
{
	char *p;

	for (p = *str; *p && *p == ' '; p++)
		;

	*str = p;

	return !!*p;
}

//TODO: CASEMAPPING,
//        - if `ascii` only az->AZ is used for nick/channel comp
static inline int
irc_toupper(const int c)
{
	/* RFC 2812, section 2.2
	 *
	 * Because of IRC's Scandinavian origin, the characters {}|^ are
	 * considered to be the lower case equivalents of the characters []\~,
	 * respectively. This is a critical issue when determining the
	 * equivalence of two nicknames or channel names.
	 */

	switch (c) {
		case '{': return '[';
		case '}': return ']';
		case '|': return '\\';
		case '^': return '~';
		default:
			return (c >= 'a' && c <= 'z') ? (c + 'A' - 'a') : c;
	}
}

int
irc_isnickchar(char c, int first)
{
	/* RFC 2812, section 2.3.1
	 *
	 * nickname   =  ( letter / special ) *8( letter / digit / special / "-" )
	 * letter     =  %x41-5A / %x61-7A       ; A-Z / a-z
	 * digit      =  %x30-39                 ; 0-9
	 * special    =  %x5B-60 / %x7B-7D       ; "[", "]", "\", "`", "_", "^", "{", "|", "}"
	 */

	return ((c >= 0x41 && c <= 0x7D) || (!first && ((c >= 0x30 && c <= 0x39) || c == '-')));
}

int
irc_ischanchar(char c, int first)
{
	/* RFC 2812, section 2.3.1
	 *
	 * channel    =  ( "#" / "+" / ( "!" channelid ) / "&" ) chanstring
	 *               [ ":" chanstring ]
	 * chanstring =  %x01-07 / %x08-09 / %x0B-0C / %x0E-1F / %x21-2B
	 * chanstring =/ %x2D-39 / %x3B-FF
	 *                 ; any octet except NUL, BELL, CR, LF, " ", "," and ":"
	 * channelid  = 5( %x41-5A / digit )   ; 5( A-Z / 0-9 )
	 */

	/* TODO: */
	(void)c;
	(void)first;

	return 1;
}

int
irc_isnick(const char *str)
{
	if (!irc_isnickchar(*str++, 1))
		return 0;

	while (*str) {
		if (!irc_isnickchar(*str++, 0))
			return 0;
	}

	return 1;
}

int
irc_ischan(const char *str)
{
	if (!irc_ischanchar(*str++, 1))
		return 0;

	while (*str) {
		if (!irc_ischanchar(*str++, 0))
			return 0;
	}

	return 1;
}

int
irc_strcmp(const char *s1, const char *s2)
{
	/* Case insensitive comparison of strings s1, s2 in accordance
	 * with RFC 2812, section 2.2
	 */

	int c1, c2;

	for (;;) {

		c1 = irc_toupper(*s1++);
		c2 = irc_toupper(*s2++);

		if ((c1 -= c2))
			return -c1;

		if (c2 == 0)
			break;
	}

	return 0;
}

int
irc_strncmp(const char *s1, const char *s2, size_t n)
{
	/* Case insensitive comparison of strings s1, s2 in accordance
	 * with RFC 2812, section 2.2, up to n characters
	 */

	int c1, c2;

	while (n > 0) {

		c1 = irc_toupper(*s1++);
		c2 = irc_toupper(*s2++);

		if ((c1 -= c2))
			return -c1;

		if (c2 == 0)
			break;

		n--;
	}

	return 0;
}

/* TODO:
 * - char *[] for args, remove getarg from message handling
 * - analogous function for parsing ctcp messages */

/* FIXME:
 *
 * in order to fully implement the protocol, trailing needs to be considered
 * just another parameter, e.g. receiving
 *   - `<from> PRIVMSG hi`
 *   - `<from> PRIVMSG :hi`
 * are identical, and currently this would break rirc's parsing
 *
 * getarg(parsed message) -> return the next arg pointer, or if current
 * arg pointer is the trailing arg, parse out a token
 */
int
parse_mesg(struct parsed_mesg *pm, char *mesg)
{
	/* Parse a string into components. Null terminators are only inserted
	 * once the message is determined to be valid
	 *
	 * RFC 2812, section 2.3.1
	 *
	 * message    =   [ ":" prefix SPACE ] command [ params ] crlf
	 * prefix     =   servername / ( nickname [ [ "!" user ] "@" host ] )
	 * command    =   1*letter / 3digit
	 * params     =   *14( SPACE middle ) [ SPACE ":" trailing ]
	 *            =/  14( SPACE middle ) [ SPACE [ ":" ] trailing ]
	 *
	 * nospcrlfcl =   %x01-09 / %x0B-0C / %x0E-1F / %x21-39 / %x3B-FF
	 *                ; any octet except NUL, CR, LF, " " and ":"
	 * middle     =   nospcrlfcl *( ":" / nospcrlfcl )
	 * trailing   =   *( ":" / " " / nospcrlfcl )
	 *
	 * SPACE      =   %x20        ; space character
	 * crlf       =   %x0D %x0A   ; "carriage return" "linefeed"
	 */

	char *end_from = NULL,
	     *end_host = NULL;

	memset(pm, 0, sizeof(*pm));

	if (*mesg == ':' && *(++mesg) != ' ') {

		/* Prefix:
		 *  =  servername
		 *  =/ nickname
		 *  =/ nickname@host
		 *  =/ nickname!user@host
		 *
		 * So:
		 *  pm->from = servername / nickname
		 *  pm->host = host / user@host
		 */

		pm->from = mesg;

		while (*mesg && *mesg != ' '  && *mesg != '!' && *mesg != '@')
			mesg++;

		if (*mesg == '!' || *mesg == '@') {
			end_from = mesg++;
			pm->host = mesg;

			while (*mesg && *mesg != ' ')
				mesg++;
		}

		end_host = mesg;
	}

	/* The command is minimally required for a valid message */
	if (!(pm->command = getarg(&mesg, " ")))
		return 0;

	if (end_from)
		*end_from = '\0';

	if (end_host)
		*end_host = '\0';

	/* Keep track of the last arg so it can be terminated */
	char *param_end = NULL;

	int param_count = 0;

	while (str_trim(&mesg)) {

		/* Maximum number of parameters found */
		if (param_count == 14) {
			pm->trailing = mesg;
			break;
		}

		/* Trailing section found */
		if (*mesg == ':') {
			pm->trailing = (mesg + 1);
			break;
		}

		if (!pm->params)
			pm->params = mesg;

		while (*mesg && *mesg != ' ')
			mesg++;

		param_count++;
		param_end = mesg;
	}

	/* Terminate the last parameter if any */
	if (param_end)
		*param_end = '\0';

	return 1;
}

int
check_pinged(const char *mesg, const char *nick)
{

	int len = strlen(nick);

	while (*mesg) {

		/* skip any prefixing characters that wouldn't match a valid nick */
		while (!(*mesg >= 0x41 && *mesg <= 0x7D))
			mesg++;

		/* nick prefixes the word, following character is space or symbol */
		if (!irc_strncmp(mesg, nick, len) && !irc_isnickchar(*(mesg + len), 0))
			return 1;

		/* skip to end of word */
		while (*mesg && *mesg != ' ')
			mesg++;
	}

	return 0;
}

char*
word_wrap(int n, char **str, char *end)
{
	/* Greedy word wrap algorithm.
	 *
	 * Given a string bounded by [start, end), return a pointer to the character one
	 * past the maximum printable character for this string segment and advance the string
	 * pointer to the next printable character or the null terminator.
	 *
	 * For example, with 7 text columns and the string "wrap     testing":
	 *
	 *     word_wrap(7, &str, str + strlen(str));
	 *
	 *              split here
	 *                  |
	 *                  v
	 *            .......
	 *           "wrap     testing"
	 *                ^    ^
	 *     returns ___|    |___ str
	 *
	 * A subsequent call to wrap on the remainder, "testing", yields the case
	 * where the whole string fits and str is advanced to the end and returned.
	 *
	 * The caller should check that (str != end) before subsequent calls
	 */

	char *ret, *tmp;

	if (n < 1)
		fatal("insufficient columns: %d", n);

	/* All fits */
	if ((end - *str) <= n)
		return (*str = end);

	/* Find last occuring ' ' character */
	ret = (*str + n);

	while (ret > *str && *ret != ' ')
		ret--;

	/* Nowhere to wrap */
	if (ret == *str)
		return (*str = ret + n);

	/* Discard whitespace between wraps */
	tmp = ret;

	while (ret > *str && *(ret - 1) == ' ')
		ret--;

	while (*tmp == ' ')
		tmp++;

	*str = tmp;

	return ret;
}

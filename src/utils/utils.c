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

int
irc_message_param(struct irc_message *m, char **param)
{
	*param = NULL;

	if (m->params == NULL)
		return 0;

	if (!str_trim(&m->params))
		return 0;

	if (!m->split && m->n_params >= 14) {
		*param = m->params;
		m->params = NULL;
		return 1;
	}

	if (*m->params == ':') {
		*param = m->params + 1;
		m->params = NULL;
		return 1;
	}

	m->n_params++;

	*param = m->params;

	while (*m->params && *m->params != ' ')
		m->params++;

	if (*m->params)
		*m->params++ = 0;

	return 1;
}

int
irc_message_parse(struct irc_message *m, char *buf, size_t len)
{
	/* RFC 2812, section 2.3.1
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

	UNUSED(len);

	memset(m, 0, sizeof(*m));

	if (!str_trim(&buf))
		return 0;

	if (*buf == ':') {

		/* Prefix:
		 *  =  :name
		 *  =/ :name@host
		 *  =/ :name!user@host
		 */

		buf++;

		m->from = buf;

		while (*buf && *buf != ' '  && *buf != '!' && *buf != '@')
			buf++;

		m->len_from = buf - m->from;

		if (m->len_from == 0)
			return 0;

		if (*buf == '!' || *buf == '@') {
			*buf++ = 0;
			m->host = buf;

			while (*buf && *buf != ' ')
				buf++;

			m->len_host = buf - m->host;
		}

		if (*buf == ' ')
			*buf++ = 0;
	}

	if (!str_trim(&buf))
		return 0;

	m->command = buf;

	while (*buf && *buf != ' ')
		buf++;

	m->len_command = buf - m->command;

	if (*buf == ' ')
		*buf++ = 0;

	if (str_trim(&buf))
		m->params = buf;

	return 1;
}

int
irc_message_split(struct irc_message *m, char **trailing)
{
	/* Split the message params and trailing arg for use in generic handling */

	*trailing = NULL;

	if (!m->params)
		return 0;

	m->split = 1;

	for (char *p = m->params; *p;) {

		while (*p == ' ')
			p++;

		if (*p == 0)
			return 0;

		m->n_params++;

		if (m->n_params >= 15) {
			if (m->params == p) {
				m->params = NULL;
				*trailing = p;
			} else {
				*(p - 1) = 0;
				*trailing = (*p) ? p : NULL;
			}
			return 1;
		}

		if (*p == ':') {
			*p++ = 0;
			*trailing = (*p) ? p : NULL;
			return 1;
		}

		while (*p && *p != ' ')
			p++;
	}

	return 0;
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

#include "src/utils/utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static inline int irc_ischanchar(char, int);
static inline int irc_isnickchar(char, int);
static inline int irc_toupper(enum casemapping, int);

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
irc_pinged(enum casemapping cm, const char *mesg, const char *nick)
{
	size_t len = strlen(nick);

	while (*mesg) {

		while (*mesg && *mesg != *nick && !irc_isnickchar(*mesg, 1))
			mesg++;

		if (!irc_strncmp(cm, mesg, nick, len) && !irc_isnickchar(*(mesg + len), 0))
			return 1;

		while (*mesg && *mesg != ' ')
			mesg++;
	}

	return 0;
}

int
irc_strcmp(enum casemapping cm, const char *s1, const char *s2)
{
	/* Case insensitive comparison of strings s1, s2 in accordance
	 * with RFC 2812, section 2.2 */

	int c1, c2;

	for (;;) {

		c1 = irc_toupper(cm, *s1++);
		c2 = irc_toupper(cm, *s2++);

		if ((c1 -= c2))
			return -c1;

		if (c2 == 0)
			break;
	}

	return 0;
}

int
irc_strncmp(enum casemapping cm, const char *s1, const char *s2, size_t n)
{
	/* Case insensitive comparison of strings s1, s2 in accordance
	 * with RFC 2812, section 2.2, up to n characters */

	int c1, c2;

	while (n > 0) {

		c1 = irc_toupper(cm, *s1++);
		c2 = irc_toupper(cm, *s2++);

		if ((c1 -= c2))
			return -c1;

		if (c2 == 0)
			break;

		n--;
	}

	return 0;
}

// TODO: reverse return order
// 0 success, -1 error
int
irc_message_param(struct irc_message *m, char **param)
{
	*param = NULL;

	if (m->params == NULL)
		return 0;

	if (!irc_strtrim(&m->params))
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
irc_message_parse(struct irc_message *m, char *buf)
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

	memset(m, 0, sizeof(*m));

	if (!irc_strtrim(&buf))
		return -1;

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
			return -1;

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

	if (!irc_strtrim(&buf))
		return -1;

	m->command = buf;

	while (*buf && *buf != ' ')
		buf++;

	m->len_command = buf - m->command;

	if (*buf == ' ')
		*buf++ = 0;

	if (irc_strtrim(&buf))
		m->params = buf;

	return 0;
}

int
irc_message_split(struct irc_message *m, const char **params, const char **trailing)
{
	/* Split the message params and trailing arg for use in generic handling */

	*params = m->params;
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
				*params = m->params = NULL;
				*trailing = p;
			} else {
				*trailing = (*p) ? p : NULL;
				do {
					if (p == m->params) {
						*params = m->params = NULL;
						return 1;
					}
					p--;
				} while (*p == ' ');
				*(p + 1) = 0;
			}
			return 1;
		}

		if (*p == ':') {
			*p = 0;
			*trailing = (*(p + 1)) ? (p + 1) : NULL;
			do {
				if (p == m->params) {
					*params = m->params = NULL;
					return 1;
				}
				p--;
			} while (*p == ' ');
			*(p + 1) = 0;
			return 1;
		}

		while (*p && *p != ' ')
			p++;
	}

	return 0;
}

char*
irc_strsep(char **str)
{
	char *p;
	char *ret;

	if (str == NULL || (p = *str) == NULL)
		return NULL;

	if ((ret = irc_strtrim(&p)) == NULL)
		return NULL;

	while (*p && *p != ' ')
		p++;

	if (*p) {
		*p = 0;
		*str = (p + 1);
	} else {
		*str = NULL;
	}

	return *ret ? ret : NULL;
}

char*
irc_strtrim(char **str)
{
	char *p;

	if (*str == NULL)
		return NULL;

	for (p = *str; *p && *p == ' '; p++)
		;

	*str = p;

	return *p ? p : NULL;
}

char*
irc_strwrap(unsigned n, char **str, char *end)
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

static inline int
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

	if (first)
		return (c == '#' || c == '+' || c == '&');

	switch (c) {
		case 0x00: /* NUL */
		case 0x07: /* BEL */
		case 0x0D: /* CR */
		case 0x0A: /* LF */
		case ' ':
		case ',':
		case ':':
			return 0;
		default:
			return 1;
	}
}

static inline int
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

static inline int
irc_toupper(enum casemapping cm, int c)
{
	/* RFC 2812, section 2.2
	 *
	 * Because of IRC's Scandinavian origin, the characters {}|^ are
	 * considered to be the lower case equivalents of the characters []\~,
	 * respectively. This is a critical issue when determining the
	 * equivalence of two nicknames or channel names. */

	switch (cm) {
		case CASEMAPPING_RFC1459:
			if (c == '^') return '~';
			/* FALLTHROUGH */
		case CASEMAPPING_STRICT_RFC1459:
			if (c == '{') return '[';
			if (c == '}') return ']';
			if (c == '|') return '\\';
			/* FALLTHROUGH */
		case CASEMAPPING_ASCII:
			return (c >= 'a' && c <= 'z') ? (c + 'A' - 'a') : c;
		default:
			fatal("Unknown CASEMAPPING");
	}
}

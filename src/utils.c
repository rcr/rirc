#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>

#include "utils.h"

static int irc_isnickchar(const char);

void
error(int errnum, const char *fmt, ...)
{
	/* Report an error and exit the program */

	va_list ap;

	fflush(stdout);

	fputs("rirc: ", stderr);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (errnum)
		fprintf(stderr, " (errno: %s)\n", strerror(errnum));
	else
		fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

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
	char *ret;

	if ((ret = malloc(strlen(str) + 1)) == NULL)
		fatal("malloc");

	strcpy(ret, str);

	return ret;
}

static int
irc_isnickchar(const char c)
{
	/* RFC 2812, section 2.3.1
	 *
	 * nickname   =  ( letter / special ) *8( letter / digit / special / "-" )
	 * letter     =  %x41-5A / %x61-7A       ; A-Z / a-z
	 * digit      =  %x30-39                 ; 0-9
	 * special    =  %x5B-60 / %x7B-7D       ; "[", "]", "\", "`", "_", "^", "{", "|", "}"
	 */

	return (c == '-' || (c >= 0x30 && c <= 0x39) || (c >= 0x41 && c <= 0x7D));
}

parsed_mesg*
parse(parsed_mesg *p, char *mesg)
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

	memset(p, 0, sizeof(parsed_mesg));

	/* Skip leading whitespace */
	while (*mesg && *mesg == ' ')
		mesg++;

	/* Check for prefix and parse if detected */
	if (*mesg == ':') {

		p->from = ++mesg;

		while (*mesg) {
			if (*mesg == '!' || (*mesg == '@' && !p->hostinfo)) {
				*mesg++ = '\0';
				p->hostinfo = mesg;
			} else if (*mesg == ' ') {
				*mesg++ = '\0';
				break;
			}
			mesg++;
		}
	}

	/* The command is minimally required for a valid message */
	if (!(p->command = getarg(&mesg, " ")))
		return NULL;

	/* Keep track of the last arg so it can be terminated */
	char *param_end = NULL;

	int param_count = 0;

	while (*mesg) {

		/* Skip whitespace before each parameter */
		while (*mesg && *mesg == ' ')
			mesg++;

		/* Parameter found */
		if (*mesg) {

			/* Maximum number of parameters found */
			if (param_count == 14) {
				p->trailing = mesg;
				break;
			}

			/* Trailing section found */
			if (*mesg == ':') {
				p->trailing = (mesg + 1);
				break;
			}

			if (!p->params)
				p->params = mesg;
		}

		while (*mesg && *mesg != ' ')
			mesg++;

		param_count++;
		param_end = mesg;
	}

	/* Terminate the last parameter if any */
	if (param_end)
		*param_end = '\0';

	return p;
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
		if (!strncasecmp(mesg, nick, len) && !irc_isnickchar(*(mesg + len))) {
			putchar('\a');
			return 1;
		}

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
		fatal("insufficient columns");

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


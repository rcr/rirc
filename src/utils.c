#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "common.h"

#define H(N) (N == NULL ? 0 : N->height)
#define MAX(A, B) (A > B ? A : B)

static int comp;
static node* new_node(char*);
static node* rotate_l(node*);
static node* rotate_r(node*);
static node* node_delete(node*, char*);
static node* node_insert(node*, char*);

static char errbuff[BUFFSIZE];

static jmp_buf jmpbuf;

void
clear_channel(channel *c)
{
	free(c->buffer_head->text);

	c->buffer_head->text = NULL;

	draw(D_BUFFER);
}

char*
errf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(errbuff, BUFFSIZE, fmt, ap);
	va_end(ap);

	return errbuff;
}

/* TODO: Tests for this function */
char*
getarg(char **str, int set_null)
{
	char *ptr, *ret;

	/* Check that str isnt pointing to NULL */
	if (!(ptr = *str))
		return NULL;
	else while (*ptr && *ptr == ' ')
		ptr++;

	if (*ptr)
		ret = ptr;
	else
		return NULL;

	while (*ptr && *ptr != ' ')
		ptr++;

	if (set_null && *ptr == ' ')
		*ptr++ = '\0';

	*str = ptr;

	return ret;
}

char*
strdup(const char *str)
{
	char *ret;

	if ((ret = malloc(strlen(str) + 1)) == NULL)
		fatal("strdup - malloc");

	strcpy(ret, str);

	return ret;
}

char*
strdupf(const char *fmt, ...)
{
	char *ret;

	if ((ret = malloc(BUFFSIZE)) == NULL)
		fatal("strdupf - malloc");

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(ret, BUFFSIZE, fmt, ap);
	va_end(ap);

	return ret;
}

/* FIXME:
 *
 * Parsing of the 15 arg max doesn't work correctly
 *
 * */
int
parse(parsed_mesg *p, char *mesg)
{
	/* RFC 2812, section 2.3.1 */
	/* message = [ ":" prefix SPACE ] command [ params ] crlf */
	/* nospcrlfcl =  %x01-09 / %x0B-0C / %x0E-1F / %x21-39 / %x3B-FF */
	/* middle =  nospcrlfcl *( ":" / nospcrlfcl ) */

	*p = (parsed_mesg){0};

	/* prefix = servername / ( nickname [ [ "!" user ] "@" host ] ) */

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

	/* command = 1*letter / 3digit */
	if (!(p->command = getarg(&mesg, 1)))
		return 0;

	/* params = *14( SPACE middle ) [ SPACE ":" trailing ] */
	/* params =/ 14( SPACE middle ) [ SPACE [ ":" ] trailing ] */
	/* trailing   =  *( ":" / " " / nospcrlfcl ) */

	char *param;
	if ((param = getarg(&mesg, 0))) {
		if (*param == ':') {
			p->params = NULL;
			*param++ = '\0';
			p->trailing = param;
		} else {
			p->params = param;

			int paramcount = 1;
			while ((param = getarg(&mesg, 0))) {
				if (paramcount == 14 || *param == ':') {
					*param++ = '\0';
					p->trailing = param;
					break;
				}
				paramcount++;
			}
		}
	}

	return 1;
}

/* TODO:
 * Consider cleaning up the policy here. Ideally a match should be:
 * match = nick *[chars] (space / null)
 * chars = Any printable characters not allowed in a nick by this server */
int
check_pinged(char *mesg, char *nick)
{
	int len = strlen(nick);

	while (*mesg) {

		/* skip to next word */
		while (*mesg == ' ')
			mesg++;

		/* nick prefixes the word, following character is space or symbol */
		if (!strncasecmp(mesg, nick, len) && !isalnum(*(mesg + len))) {
			putchar('\a');
			return 1;
		}

		/* skip to end of word */
		while (*mesg && *mesg != ' ')
			mesg++;
	}

	return 0;
}

static node*
rotate_r(node *x)
{
	node *y = x->l;
	node *z = y->r;
	y->r = x;
	x->l = z;

	x->height = MAX(H(x->l), H(x->r)) + 1;
	y->height = MAX(H(y->l), H(y->r)) + 1;

	return y;
}

static node*
rotate_l(node *x)
{
	node *y = x->r;
	node *z = y->l;
	y->l = x;
	x->r = z;

	x->height = MAX(H(x->l), H(x->r)) + 1;
	y->height = MAX(H(y->l), H(y->r)) + 1;

	return y;
}

static node*
new_node(char *nick)
{
	node *n;
	if ((n = malloc(sizeof(node))) == NULL)
		fatal("new_node");
	n->l = NULL;
	n->r = NULL;
	n->height = 1;
	strcpy(n->nick, nick);

	return n;
}

void
free_nicklist(node *n)
{
	if (n == NULL)
		return;

	free_nicklist(n->l);
	free_nicklist(n->r);
	free(n);
}

int
nicklist_insert(node **n, char *nick)
{
	if (!setjmp(jmpbuf))
		*n = node_insert(*n, nick);
	else
		return 0;

	return 1;
}

int
nicklist_delete(node **n, char *nick)
{
	if (!setjmp(jmpbuf))
		*n = node_delete(*n, nick);
	else
		return 0;

	return 1;
}

static node*
node_insert(node *n, char *nick)
{
	if (n == NULL)
		return new_node(nick);

	if (!(comp = strcasecmp(nick, n->nick)))
		longjmp(jmpbuf, 1);
	else if (comp > 1)
		n->r = node_insert(n->r, nick);
	else
		n->l = node_insert(n->l, nick);

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* Rebalance */
	if (balance > 1) {
		if (strcasecmp(nick, n->l->nick) > 1)
			n->l = rotate_l(n->l);

		return rotate_r(n);
	}
	if (balance < -1) {
		if (strcasecmp(n->r->nick, nick) > 1)
			n->r = rotate_r(n->r);

		return rotate_l(n);
	}

	return n;
}

static node*
node_delete(node *n, char *nick)
{
	if (n == NULL)
		longjmp(jmpbuf, 1);

	if (!(comp = strcasecmp(nick, n->nick))) {

		if (n->l && n->r) {

			node *temp = n->r;
			while (temp->l)
				temp = temp->l;

			strcpy(n->nick, temp->nick);

			n->r = node_delete(n->r, temp->nick);
		} else {

			node *temp = n->l ? n->l : n->r;

			if (temp == NULL) {
				temp = n;
				n = NULL;
			} else {
				*n = *temp;
			}

			free(temp);
		}
	}
	else if (comp > 1)
		n->r = node_delete(n->r, nick);
	else
		n->l = node_delete(n->l, nick);

	if (n == NULL)
		return n;

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* Rebalance */
	if (balance > 1) {
		if (H(n->l->l) - H(n->l->r) < 0)
			n->l =  rotate_l(n->l);

		return rotate_r(n);
	}
	if (balance < -1) {
		if (H(n->r->l) - H(n->r->r) > 0)
			n->r = rotate_r(n->r);

		return rotate_l(n);
	}

	return n;
}

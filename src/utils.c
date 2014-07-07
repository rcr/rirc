#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>

#include "common.h"

#define H(n) (n == NULL ? 0 : n->height)
#define MAX(a, b) (a > b ? a : b)

int nick_cmp(char*, char*);
node* new_node(char*);
node* rotate_l(node*);
node* rotate_r(node*);
node* node_delete(node*, char*);
node* node_insert(node*, char*);

char errbuff[BUFFSIZE];

static jmp_buf jmpbuf;

char*
errf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(errbuff, BUFFSIZE-1, fmt, args);
	va_end(args);
	return errbuff;
}

int
getarg(char **arg, char **str)
{
	char *i = *str;

	while (*i == ' ')
		i++;

	if (*i == '\0')
		return 0;

	if (*i == ':') {
		i++;
		if (*i == '\0')
			return 0;
		else {
			*arg = i;
			return 2;
		}
	}

	*arg = i;

	while (*i != ' ' && *i != '\0')
		i++;

	if (*i == ' ')
		*i++ = '\0';

	*str = i;

	return 1;
}

int
getargc(char **arg, char **str, char c)
{
	char *i = *str;

	while (*i == ' ' || *i == ':')
		i++;

	if (*i == '\0')
		return 0;

	*arg = i;

	while (*i != c && *i != '\0')
		i++;

	if (*i == c)
		*i++ = '\0';

	*str = i;

	return 1;
}

/* TODO: replacing getarg */
char*
getarg_(char **mesg, int set_null)
{
	char *ret, *ptr = *mesg;

	while (*ptr && *ptr == ' ')
		ptr++;

	ret = ptr;

	while (*ptr && *ptr != ' ')
		ptr++;

	if (set_null && *ptr == ' ')
		*ptr++ = '\0';

	*mesg = ptr;

	return ret;
}

struct parsed_mesg*
parse(char *mesg)
{
	/* RFC 2812, section 2.3.1 */
	/* message = [ ":" prefix SPACE ] command [ params ] crlf */
	/* nospcrlfcl =  %x01-09 / %x0B-0C / %x0E-1F / %x21-39 / %x3B-FF */
	/* middle =  nospcrlfcl *( ":" / nospcrlfcl ) */

	/* prefix = servername / ( nickname [ [ "!" user ] "@" host ] ) */
	parsed.from = parsed.hostinfo = NULL;
	if (*mesg == ':') {
		parsed.from = ++mesg;

		while (*mesg) {
			if (*mesg == '!') {
				*mesg++ = '\0';
				parsed.hostinfo = mesg;
			} else if (*mesg == '@' && !parsed.hostinfo) {
				*mesg++ = '\0';
				parsed.hostinfo = mesg;
			} else if (*mesg == ' ') {
				*mesg++ = '\0';
				break;
			}
			mesg++;
		}
	}

	/* command = 1*letter / 3digit */
	parsed.command = getarg_(&mesg, 1);

	/* params = *14( SPACE middle ) [ SPACE ":" trailing ] */
	/* params =/ 14( SPACE middle ) [ SPACE [ ":" ] trailing ] */
	/* trailing   =  *( ":" / " " / nospcrlfcl ) */
	char *param;
	if ((param = getarg_(&mesg, 0))) {
		if (*param == ':') {
			parsed.params = NULL;
			*param++ = '\0';
			parsed.trailing = param;
		} else {
			parsed.params = param;

			int paramcount = 1;
			while ((param = getarg_(&mesg, 0))) {
				if (paramcount == 14 || *param == ':') {
					*param++ = '\0';
					parsed.trailing = param;
					break;
				}
				paramcount++;
			}
		}
	}

	return &parsed;
}
int
cmdcmp(char *i, char *cmd)
{
	while (*cmd++ == *i++)
		if (*cmd == '\0' && (*i == '\0' || *i == ' '))
			return 1;
	return 0;
}

int
cmdcmpc(char *i, char *cmd)
{
	while (*cmd++ == toupper(*i++))
		if (*cmd == '\0' && (*i == '\0' || *i == ' '))
			return 1;
	return 0;
}

int
check_pinged(char *mesg, char *nick)
{
	char *n;

	while (*mesg != '\0') {

		n = nick;
		while (*n == *mesg) {
			n++, mesg++;

			if (*n == '\0') {
				if (!isalnum(*mesg)) {
					putchar(0x7); /* BEL character */
					return 1;
				}
				break;
			}
		}

		while (*mesg != ' ' && *mesg != '\0')
			mesg++;

		while (*mesg == ' ' && *mesg != '\0')
			mesg++;
	}
	return 0;
}

node*
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

node*
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

node*
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
nick_cmp(char *n1, char *n2)
{
	while (*n1 == *n2 && *n1 != '\0')
		n1++, n2++;

	if (*n1 > *n2)
		return 1;
	if (*n1 < *n2)
		return 0;

	return -1;
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

node*
node_insert(node *n, char *nick)
{
	if (n == NULL)
		return new_node(nick);

	int comp;
	if ((comp = nick_cmp(nick, n->nick)) == -1)
		longjmp(jmpbuf, 1);
	else if (comp)
		n->r = node_insert(n->r, nick);
	else
		n->l = node_insert(n->l, nick);

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* Rebalance */
	if (balance > 1) {
		if (nick_cmp(nick, n->l->nick))
			n->l = rotate_l(n->l);

		return rotate_r(n);
	}
	if (balance < -1) {
		if (nick_cmp(n->r->nick, nick))
			n->r = rotate_r(n->r);

		return rotate_l(n);
	}

	return n;
}

node*
node_delete(node *n, char *nick)
{
	if (n == NULL)
		longjmp(jmpbuf, 1);

	int comp;
	if ((comp = nick_cmp(nick, n->nick)) == -1) {

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
	else if(comp)
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

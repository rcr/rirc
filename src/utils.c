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

/* AVL tree function */
static avl_node* _avl_add(avl_node*, const char*, void*);
static avl_node* _avl_del(avl_node*, const char*);
static avl_node* _avl_get(avl_node*, const char*, size_t);
static avl_node* avl_new_node(const char*, void*);
static avl_node* avl_rotate_L(avl_node*);
static avl_node* avl_rotate_R(avl_node*);

static jmp_buf jmpbuf;

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
getarg(char **str, const char sep)
{
	/* Return a token parsed from *str delimited by sep.
	 *
	 * Consumes all sep characters preceding the token and null terminates it.
	 *
	 * Returns NULL if *str is NULL or contains only sep characters */

	char *ret, *ptr;

	if (str == NULL || (ptr = *str) == NULL)
		return NULL;

	while (*ptr && *ptr == sep)
		ptr++;

	if (*ptr == '\0')
		return NULL;

	ret = ptr;

	while (*ptr && *ptr != sep)
		ptr++;

	/* If the string continues after the found arg, set the input to point
	 * one past the arg's null terminator.
	 *
	 * This might result in *str pointing to the original string's null
	 * terminator, in which case the next call to getarg will return NULL */
	*str = ptr + (*ptr == sep);

	*ptr = '\0';

	return ret;
}

void
auto_nick(char **autonick, char *nick)
{
	char *p = *autonick;
	while (*p == ' ' || *p == ',')
		p++;

	if (*p == '\0') {

		/* Autonicks exhausted, generate a random nick */
		char *base = "rirc_";
		char *cset = "0123456789ABCDEF";

		strcpy(nick, base);
		nick += strlen(base);

		int i, len = strlen(cset);
		for (i = 0; i < 4; i++)
			*nick++ = cset[rand() % len];
	} else {
		int c = 0;
		while (*p != ' ' && *p != ',' && *p != '\0' && c++ < NICKSIZE)
			*nick++ = *p++;
		*autonick = p;
	}

	*nick = '\0';
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
	if (!(p->command = getarg(&mesg, ' ')))
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
	if (param_end) {

		while (*param_end && *param_end != ' ')
			param_end++;

		*param_end = '\0';
	}

	return p;
}

/* TODO:
 * Consider cleaning up the policy here. Ideally a match should be:
 * match = nick *[chars] (space / null)
 * chars = Any printable characters not allowed in a nick by this server */
int
check_pinged(const char *mesg, const char *nick)
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

char*
word_wrap(int text_cols, char **ptr1, char *ptr2)
{
	/* Greedy word wrap algorithm.
	 *
	 * Given a string bounded by [start, end), return a pointer to the
	 * character one past the maximum printable character for this string segment
	 * within text_cols wrapped on whitespace, and set ptr1 to the first character
	 * that is printable on the next line.
	 *
	 * This algorithm never discards whitespace at the beginning of lines, but
	 * does discard whitespace between line continuations and at end of lines.
	 *
	 * text_cols: the number of printable columns
	 * ptr1:      the first character in string
	 * ptr2:      the string's null terminator
	 */

	char *tmp, *ret = (*ptr1) + text_cols;

	if (text_cols <= 0)
		fatal("Insufficient columns");

	/* Entire line fits within text_cols */
	if (ret >= ptr2)
		return (*ptr1 = ptr2);

	/* At least one char exists that can print on current line */

	if (*ret == ' ') {

		/* Wrap on this space, find printable character for next line */
		for (tmp = ret; *tmp == ' '; tmp++)
			;

		*ptr1 = tmp;

	} else {

		/* Find a space to wrap on, or wrap on */
		for (tmp = (*ptr1) + 1; *ret != ' ' && ret > tmp; ret--)
			;

		/* No space found, wrap on entire segment */
		if (ret == tmp)
			return (*ptr1 = (*ptr1) + text_cols);

		*ptr1 = ret + 1;
	}

	return ret;
}

int
count_line_rows(int text_cols, buffer_line *l)
{
	/* Count the number of times a line will wrap within text_cols columns */

	int count = 0;

	char *ptr1 = l->text;
	char *ptr2 = l->text + l->len;

	do {
		word_wrap(text_cols, &ptr1, ptr2);
		count++;
	} while (*ptr1);

	return count;
}

/* AVL tree functions */

void
free_avl(avl_node *n)
{
	/* Recusrively free an AVL tree */

	if (n == NULL)
		return;

	free_avl(n->l);
	free_avl(n->r);
	free(n->key);
	free(n->val);
	free(n);
}

int
avl_add(avl_node **n, const char *key, void *val)
{
	/* Entry point for adding a node to an AVL tree */

	if (setjmp(jmpbuf))
		return 0;

	*n = _avl_add(*n, key, val);

	return 1;
}

int
avl_del(avl_node **n, const char *key)
{
	/* Entry point for removing a node from an AVL tree */

	if (setjmp(jmpbuf))
		return 0;

	*n = _avl_del(*n, key);

	return 1;
}

const avl_node*
avl_get(avl_node *n, const char *key, size_t len)
{
	/* Entry point for fetching an avl node with prefix key */

	if (setjmp(jmpbuf))
		return NULL;

	return _avl_get(n, key, len);
}

static avl_node*
avl_new_node(const char *key, void *val)
{
	avl_node *n;

	if ((n = calloc(1, sizeof(*n))) == NULL)
		fatal("calloc");

	n->height = 1;
	n->key = strdup(key);
	n->val = val;

	return n;
}

static avl_node*
avl_rotate_R(avl_node *r)
{
	/* Rotate right for root r and pivot p
	 *
	 *     r          p
	 *    / \   ->   / \
	 *   p   c      a   r
	 *  / \            / \
	 * a   b          b   c
	 *
	 */

	avl_node *p = r->l;
	avl_node *b = p->r;

	p->r = r;
	r->l = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static avl_node*
avl_rotate_L(avl_node *r)
{
	/* Rotate left for root r and pivot p
	 *
	 *   r            p
	 *  / \    ->    / \
	 * a   p        r   c
	 *    / \      / \
	 *   b   c    a   b
	 *
	 */

	avl_node *p = r->r;
	avl_node *b = p->l;

	p->l = r;
	r->r = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static avl_node*
_avl_add(avl_node *n, const char *key, void *val)
{
	/* Recursively add key to an AVL tree.
	 *
	 * If a duplicate is found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		return avl_new_node(key, val);

	int ret = strcasecmp(key, n->key);

	if (ret == 0)
		/* Duplicate found */
		longjmp(jmpbuf, 1);

	else if (ret > 0)
		n->r = _avl_add(n->r, key, val);

	else if (ret < 0)
		n->l = _avl_add(n->l, key, val);

	/* Node was successfully added, recaculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (strcasecmp(key, n->l->key) > 0)
			n->l = avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (strcasecmp(n->r->key, key) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static avl_node*
_avl_del(avl_node *n, const char *key)
{
	/* Recursive function for deleting nodes from an AVL tree
	 *
	 * If the node isn't found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		/* Node not found */
		longjmp(jmpbuf, 1);

	int ret = strcasecmp(key, n->key);

	if (ret == 0) {
		/* Node found */

		if (n->l && n->r) {
			/* Recusrively delete nodes with both children to ensure balance */

			/* Find the next largest value in the tree (the leftmost node in the right subtree) */
			avl_node *next = n->r;

			while (next->l)
				next = next->l;

			/* Swap it's value with the node being deleted */
			char *t = n->key;

			n->key = next->key;
			next->key = t;

			/* Recusively delete in the right subtree */
			n->r = _avl_del(n->r, t);

		} else {
			/* If n has a child, return it */
			avl_node *tmp = (n->l) ? n->l : n->r;

			free(n);

			return tmp;
		}
	}

	else if (ret > 0)
		n->r = _avl_del(n->r, key);

	else if (ret < 0)
		n->l = _avl_del(n->l, key);

	/* Node was successfully deleted, recalculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (H(n->l->l) - H(n->l->r) < 0)
			n->l =  avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (H(n->r->l) - H(n->r->r) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static avl_node*
_avl_get(avl_node *n, const char *key, size_t len)
{
	/* Case insensitive search for a node whose value is prefixed by key */

	/* Failed to find node */
	if (n == NULL)
		longjmp(jmpbuf, 1);

	int ret = strncasecmp(key, n->key, len);

	if (ret > 0)
		return _avl_get(n->r, key, len);

	if (ret < 0)
		return _avl_get(n->l, key, len);

	/* Match found */
	return n;
}

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "common.h"

#define H(n) (n == NULL ? 0 : n->height)
#define MAX(a, b) (a > b ? a : b)

int nick_cmp(char*, char*);
node* new_node(char*);
node* rotate_l(node*);
node* rotate_r(node*);
node* node_delete(node*, char*);
node* node_insert(node*, char*);

static jmp_buf jmpbuf;

node *rotate_r(node *x)
{
	node *y = x->l;
	node *T2 = y->r;
	y->r = x;
	x->l = T2;

	x->height = MAX(H(x->l), H(x->r)) + 1;
	y->height = MAX(H(y->l), H(y->r)) + 1;

	return y;
}

node *rotate_l(node *x)
{
	node *y = x->r;
	node *T2 = y->l;
	y->l = x;
	x->r = T2;

	x->height = MAX(H(x->l), H(x->r)) + 1;
	y->height = MAX(H(y->l), H(y->r)) + 1;

	return y;
}

node*
new_node(char *nick)
{
	node *n;
	if ((n = malloc(sizeof(node))) == NULL)
		fatal("insert_nick");
	n->l = NULL;
	n->r = NULL;
	n->height = 1;
	strcpy(n->nick, nick);
	return n;
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
node_insert(node *node, char *nick)
{
	if (node == NULL)
		return new_node(nick);

	int comp;
	if ((comp = nick_cmp(nick, node->nick)) == -1)
		longjmp(jmpbuf, 1);
	else if (comp)
		node->r = node_insert(node->r, nick);
	else
		node->l = node_insert(node->l, nick);

	node->height = MAX(H(node->l), H(node->r)) + 1;

	int balance = H(node->l) - H(node->r);

	/* Rebalance */
	if (balance > 1) {
		if (nick_cmp(nick, node->l->nick))
			node->l = rotate_l(node->l);

		return rotate_r(node);
	}
	if (balance < -1) {
		if (nick_cmp(node->r->nick, nick))
			node->r = rotate_r(node->r);

		return rotate_l(node);
	}

	return node;
}


node*
node_delete(node *node, char *nick)
{
	;
}

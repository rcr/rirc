#include <stdlib.h>
#include <string.h>

#define H(n) (n == NULL ? 0 : n->height)
#define MAX(a, b) (a > b ? a : b)

/* AVL Tree implementation */
typedef struct node {
	char *nick;
	int height;
	struct node *l;
	struct node *r;
} node;

typedef struct nicklist {
	int count;
	node *root;
} nicklist;

int nick_cmp(char*, char*);
node* new_node(char*);
node* rotate_l(node*);
node* rotate_r(node*);
node* tree_insert(node*, char*);

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
	node *node;
	if ((node = malloc(sizeof(node))) == NULL)
		return NULL; //		fatal("insert_nick");
	if ((node->nick = malloc(strlen(nick))) == NULL)
		return NULL; //		fatal("insert_nick");
	node->l = NULL;
	node->r = NULL;
	node->height = 1;
	strcpy(node->nick, nick);
	return node;
}

int
nick_cmp(char *n1, char *n2)
{
	for (;;) {
		if (*n1 == *n2) {
			if (*n1 == '\0')
				return -1;
			else
				n1++, n2++;
		}
		else if (*n1 > *n2)
			return 1;
		else if (*n1 < *n2)
			return 0;
	}
}

node*
tree_insert(node *node, char *nick)
{
	if (node == NULL)
		return new_node(nick);

	int comp;
	if ((comp = nick_cmp(nick, node->nick)) == -1)
		return node;
	else if (comp)
		node->r = tree_insert(node->r, nick);
	else
		node->l = tree_insert(node->l, nick);

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














/* TEST CODE */
#include <stdio.h>
#include <time.h>
#include <math.h>

int
deepest(node *n)
{
	if (n == NULL)
		return 0;
	int ll = deepest(n->l);
	int rr = deepest(n->r);
	return 1 + ((ll > rr) ? ll : rr);
}

int main(void) {
	srand(time(NULL));

	nicklist n = { .count = 0, .root = NULL, };

	int num = 100000;
	char buff[16];

	/* lowercase ascii [97 122] */
	while(num--) {
		char *ptr = buff;

		/* generate nicks between 5-15 chars long */
		int len = 5 + (rand() % 11);

		while (len--)
			*ptr++ = 97 + (rand() % 26);
		*ptr = '\0';

		n.root = tree_insert(n.root, buff);
		n.count++;
	}

	printf("Count %d\n", n.count);

	printf("2*log(n):     %g\n", 2*(log2(100000)));
	printf("deepest:      %d\n", deepest(n.root));
	printf("root height:  %d\n", n.root->height);

	return 0;
}

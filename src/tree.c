#include <setjmp.h>
#include <stdlib.h>

#include "tree.h"
#include "utils.h"

#define H(N) ((N) == NULL ? 0 : (N)->height)

//TODO: generalize avl tree functions as macros

static struct avl_node* _avl_add(jmp_buf*, struct avl_node*, const char*, int (*)(const char*, const char*), void*);
static struct avl_node* _avl_del(jmp_buf*, struct avl_node*, const char*, int (*)(const char*, const char*));
static struct avl_node* _avl_get(jmp_buf*, struct avl_node*, const char*, int (*)(const char*, const char*, size_t), size_t);
static struct avl_node* avl_new_node(const char*, void*);
static void avl_free_node(struct avl_node*);
static struct avl_node* avl_rotate_L(struct avl_node*);
static struct avl_node* avl_rotate_R(struct avl_node*);

/* AVL tree functions */

void
free_avl(struct avl_node *n)
{
	/* Recusrively free an AVL tree */

	if (n == NULL)
		return;

	free_avl(n->l);
	free_avl(n->r);
	avl_free_node(n);
}

int
avl_add(struct avl_node **n, const char *key, int (*cmp)(const char*, const char*), void *val)
{
	/* Entry point for adding a node to an AVL tree */

	jmp_buf err;

	if (setjmp(err))
		return 0;

	*n = _avl_add(&err, *n, key, cmp, val);

	return 1;
}

int
avl_del(struct avl_node **n, const char *key, int (*cmp)(const char*, const char*))
{
	/* Entry point for removing a node from an AVL tree */

	jmp_buf err;

	if (setjmp(err))
		return 0;

	*n = _avl_del(&err, *n, key, cmp);

	return 1;
}

const struct avl_node*
avl_get(struct avl_node *n, const char *key, int (*cmp)(const char*, const char*, size_t), size_t len)
{
	/* Entry point for fetching an avl node with prefix key */

	jmp_buf err;

	if (setjmp(err))
		return NULL;

	return _avl_get(&err, n, key, cmp, len);
}

static struct avl_node*
avl_new_node(const char *key, void *val)
{
	struct avl_node *n;

	if ((n = calloc(1, sizeof(*n))) == NULL)
		fatal("calloc");

	n->height = 1;
	n->key = strdup(key);
	n->val = val;

	return n;
}

static void
avl_free_node(struct avl_node *n)
{
	free(n->key);
	free(n->val);
	free(n);
}

static struct avl_node*
avl_rotate_R(struct avl_node *r)
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

	struct avl_node *p = r->l;
	struct avl_node *b = p->r;

	p->r = r;
	r->l = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static struct avl_node*
avl_rotate_L(struct avl_node *r)
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

	struct avl_node *p = r->r;
	struct avl_node *b = p->l;

	p->l = r;
	r->r = b;

	r->height = MAX(H(r->l), H(r->r)) + 1;
	p->height = MAX(H(p->l), H(p->r)) + 1;

	return p;
}

static struct avl_node*
_avl_add(jmp_buf *err, struct avl_node *n, const char *key, int (*cmp)(const char*, const char*), void *val)
{
	/* Recursively add key to an AVL tree.
	 *
	 * If a duplicate is found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		return avl_new_node(key, val);

	int ret = cmp(key, n->key);

	if (ret == 0)
		/* Duplicate found */
		longjmp(*err, 1);

	else if (ret > 0)
		n->r = _avl_add(err, n->r, key, cmp, val);

	else if (ret < 0)
		n->l = _avl_add(err, n->l, key, cmp, val);

	/* Node was successfully added, recaculate height and rebalance */

	n->height = MAX(H(n->l), H(n->r)) + 1;

	int balance = H(n->l) - H(n->r);

	/* right rotation */
	if (balance > 1) {

		/* left-right rotation */
		if (cmp(key, n->l->key) > 0)
			n->l = avl_rotate_L(n->l);

		return avl_rotate_R(n);
	}

	/* left rotation */
	if (balance < -1) {

		/* right-left rotation */
		if (cmp(n->r->key, key) > 0)
			n->r = avl_rotate_R(n->r);

		return avl_rotate_L(n);
	}

	return n;
}

static struct avl_node*
_avl_del(jmp_buf *err, struct avl_node *n, const char *key, int (*cmp)(const char*, const char*))
{
	/* Recursive function for deleting nodes from an AVL tree
	 *
	 * If the node isn't found (case insensitive) longjmp is called to indicate failure */

	if (n == NULL)
		/* Node not found */
		longjmp(*err, 1);

	int ret = cmp(key, n->key);

	if (ret == 0) {
		/* Node found */

		if (n->l && n->r) {
			/* Recursively delete nodes with both children to ensure balance */

			/* Find the next largest value in the tree (the leftmost node in the right subtree) */
			struct avl_node *next = n->r;

			while (next->l)
				next = next->l;

			/* Swap it's value with the node being deleted */
			struct avl_node t = *n;

			n->key = next->key;
			n->val = next->val;
			next->key = t.key;
			next->val = t.val;

			/* Recusively delete in the right subtree */
			n->r = _avl_del(err, n->r, t.key, cmp);

		} else {
			/* If n has a child, return it */
			struct avl_node *tmp = (n->l) ? n->l : n->r;

			avl_free_node(n);

			return tmp;
		}
	}

	else if (ret > 0)
		n->r = _avl_del(err, n->r, key, cmp);

	else if (ret < 0)
		n->l = _avl_del(err, n->l, key, cmp);

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

static struct avl_node*
_avl_get(jmp_buf *err, struct avl_node *n, const char *key, int (*cmp)(const char*, const char*, size_t), size_t len)
{
	/* Case insensitive search for a node whose value is prefixed by key */

	/* Failed to find node */
	if (n == NULL)
		longjmp(*err, 1);

	int ret = cmp(key, n->key, len);

	if (ret > 0)
		return _avl_get(err, n->r, key, cmp, len);

	if (ret < 0)
		return _avl_get(err, n->l, key, cmp, len);

	/* Match found */
	return n;
}

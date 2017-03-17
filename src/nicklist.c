#include "nicklist.h"

/* TODO:
 * redesigning how avl trees are used, testing with nicklist
 * abstractions
 * */

int
nicklist_add(struct nicklist *l, const char *nick)
{
	if (avl_add(&(l->root), nick, NULL)) {
		l->count++;
		return 1;
	}

	return 0;
}

int
nicklist_del(struct nicklist *l, const char *nick)
{
	if (avl_del(&(l->root), nick)) {
		l->count--;
		return 1;
	}

	return 0;
}

//TODO: return struct nick*
const char*
nicklist_get(struct nicklist *l, const char *nick, size_t len)
{
	struct avl_node *n;

	if ((n = avl_get(l->root, nick, len)))
		return n->key;
	else
		return NULL;
}

void
nicklist_free(struct nicklist *l)
{
	free_avl(l->root);
	l->root = NULL;
	l->count = 0;
}

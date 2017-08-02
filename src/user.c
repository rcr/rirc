#include "user.h"
#include "utils.h"

int
user_list_add(struct user_list *l, const char *nick)
{
	if (avl_add(&(l->root), nick, irc_strcmp, NULL)) {
		l->count++;
		return 1;
	}

	return 0;
}

int
user_list_del(struct user_list *l, const char *nick)
{
	if (avl_del(&(l->root), nick, irc_strcmp)) {
		l->count--;
		return 1;
	}

	return 0;
}

//TODO: return struct nick*
const char*
user_list_get(struct user_list *l, const char *nick, size_t len)
{
	const struct avl_node *n;

	if ((n = avl_get(l->root, nick, irc_strncmp, len)))
		return n->key;
	else
		return NULL;
}

void
user_list_free(struct user_list *l)
{
	free_avl(l->root);
	l->root = NULL;
	l->count = 0;
}

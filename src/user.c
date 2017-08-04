#include "user.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

#if 0
static inline int user_cmp(struct user*, struct user*);

AVL_GENERATE(_user_list, user, node, user_cmp)

static inline int
user_cmp(struct user *u1, struct user *u2)
{
	return irc_strcmp(u1->nick, u2->nick);
}

int
_user_list_add(
		struct _user_list *ul,
		const char *nick,
		const char *user,
		const char *host)
{
	struct user *u, *r;

	size_t len_n = strlen(nick),
	       len_u = strlen(user),
	       len_h = strlen(host);

	if ((u = calloc(1, sizeof(*u) + len_n + len_u + len_h + 3)) == NULL)
		fatal("calloc", errno);

	if ((r = AVL_ADD(_user_list, ul, u)) == NULL)
		free(u);
	else {
		u->nick = strcpy(u->_,         nick);
		u->user = strcpy(u->_ + len_n, user);
		u->host = strcpy(u->_ + len_u, host);

		ul->count++;
	}

	return !!r;
}

int
_user_list_del(struct _user_list *ul, const char *nick)
{
	struct user u = { .nick = nick }, *r;

	if ((r = AVL_DEL(_user_list, ul, &u))) {
		free(r);

		ul->count--;
	}

	return !!r;
}

struct user*
_user_list_get(struct _user_list *ul, const char *nick)
{
	struct user u = { .nick = nick };

	return AVL_GET(_user_list, ul, &u);
}

/* ^ WIP */
#endif

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

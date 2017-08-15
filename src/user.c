#include "user.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static inline int user_cmp(struct user*, struct user*);
static inline int user_ncmp(struct user*, struct user*, size_t);

static inline void user_free(struct user*);

AVL_GENERATE(user_list, user, node, user_cmp, user_ncmp)

static inline int
user_cmp(struct user *u1, struct user *u2)
{
	return irc_strcmp(u1->nick, u2->nick);
}

static inline int
user_ncmp(struct user *u1, struct user *u2, size_t n)
{
	return irc_strncmp(u1->nick, u2->nick, n);
}

static inline void
user_free(struct user *u)
{
	free(u);
}

int
user_list_add(struct user_list *ul, char *nick)
{
	struct user *ret, *u;

	size_t len = strlen(nick);

	if ((u = calloc(1, sizeof(*u) + len + 1)) == NULL)
		fatal("calloc", errno);

	u->nick = strcpy(u->_, nick);

	if ((ret = AVL_ADD(user_list, ul, u)) == NULL)
		user_free(u);
	else
		ul->count++;

	return !!ret;
}

int
user_list_del(struct user_list *ul, char *nick)
{
	struct user *ret, u = { .nick = nick };

	if ((ret = AVL_DEL(user_list, ul, &u)) != NULL) {
		user_free(ret);
		ul->count--;
	}

	return !!ret;
}

struct user*
user_list_get(struct user_list *ul, char *nick, size_t prefix_len)
{
	struct user u = { .nick = nick };

	if (prefix_len == 0)
		return AVL_GET(user_list, ul, &u);
	else
		return AVL_NGET(user_list, ul, &u, prefix_len);
}

void
user_list_free(struct user_list *ul)
{
	AVL_FOREACH(user_list, ul, user_free);
}

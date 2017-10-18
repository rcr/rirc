#include "user.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static struct user* user(const char*);

static inline int user_cmp(struct user*, struct user*);
static inline int user_ncmp(struct user*, struct user*, size_t);

static inline void user_free(struct user*);

AVL_GENERATE(user_list, user, node, user_cmp, user_ncmp)

static inline int
user_cmp(struct user *u1, struct user *u2)
{
	/* TODO: CASEMAPPING */
	return irc_strcmp(u1->nick, u2->nick);
}

static inline int
user_ncmp(struct user *u1, struct user *u2, size_t n)
{
	/* TODO: CASEMAPPING */
	return irc_strncmp(u1->nick, u2->nick, n);
}

static inline void
user_free(struct user *u)
{
	free(u);
}

static struct user*
user(const char *nick)
{
	struct user *u;

	if ((u = calloc(1, sizeof(*u) + strlen(nick) + 1)) == NULL)
		fatal("calloc", errno);

	u->nick = strcpy(u->_, nick);

	return u;
}

enum user_err
user_list_add(struct user_list *ul, char *nick)
{
	/* Create user and add to userlist */

	if (user_list_get(ul, nick, 0) != NULL)
		return USER_ERR_DUPLICATE;

	AVL_ADD(user_list, ul, user(nick));
	ul->count++;

	return USER_ERR_NONE;
}

enum user_err
user_list_del(struct user_list *ul, char *nick)
{
	/* Delete user and remove from userlist */

	struct user *ret;

	if ((ret = user_list_get(ul, nick, 0)) == NULL)
		return USER_ERR_NOT_FOUND;

	AVL_DEL(user_list, ul, ret);
	ul->count--;

	user_free(ret);

	return USER_ERR_NONE;
}

enum user_err
user_list_rpl(struct user_list *ul, char *nick_old, char *nick_new)
{
	/* Replace a user in a list by name, maintaining modes */

	struct user *old, *new;

	if ((old = user_list_get(ul, nick_old, 0)) == NULL)
		return USER_ERR_NOT_FOUND;

	if ((new = user_list_get(ul, nick_new, 0)) != NULL)
		return USER_ERR_DUPLICATE;

	new = user(nick_new);

	AVL_ADD(user_list, ul, new);
	AVL_DEL(user_list, ul, old);

	/* TODO: copy all modes */
	new->prefix = old->prefix;

	user_free(old);

	return USER_ERR_NONE;
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

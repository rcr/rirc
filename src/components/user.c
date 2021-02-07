#include "src/components/user.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static struct user* user(const char*, struct mode);
static inline int user_cmp(struct user*, struct user*, void *arg);
static inline int user_ncmp(struct user*, struct user*, void *arg, size_t);
static inline void user_free(struct user*);

AVL_GENERATE(user_list, user, ul, user_cmp, user_ncmp)

static inline int
user_cmp(struct user *u1, struct user *u2, void *arg)
{
	return irc_strcmp(*(enum casemapping*)arg, u1->nick, u2->nick);
}

static inline int
user_ncmp(struct user *u1, struct user *u2, void *arg, size_t n)
{
	return irc_strncmp(*(enum casemapping*)arg, u1->nick, u2->nick, n);
}

static inline void
user_free(struct user *u)
{
	free(u);
}

static struct user*
user(const char *nick, struct mode prfxmodes)
{
	size_t len = strlen(nick);
	struct user *u;

	if ((u = calloc(1, sizeof(*u) + len + 1)) == NULL)
		fatal("calloc: %s", strerror(errno));

	u->nick = memcpy(u->_, nick, len + 1);
	u->nick_len = len;
	u->prfxmodes = prfxmodes;

	return u;
}

enum user_err
user_list_add(struct user_list *ul, enum casemapping cm, const char *nick, struct mode prfxmodes)
{
	/* Create user and add to userlist */

	if (user_list_get(ul, cm, nick, 0) != NULL)
		return USER_ERR_DUPLICATE;

	AVL_ADD(user_list, ul, user(nick, prfxmodes), &cm);
	ul->count++;

	return USER_ERR_NONE;
}

enum user_err
user_list_del(struct user_list *ul, enum casemapping cm, const char *nick)
{
	/* Delete user and remove from userlist */

	struct user *u;

	if ((u = user_list_get(ul, cm, nick, 0)) == NULL)
		return USER_ERR_NOT_FOUND;

	AVL_DEL(user_list, ul, u, &cm);
	ul->count--;

	user_free(u);

	return USER_ERR_NONE;
}

enum user_err
user_list_rpl(struct user_list *ul, enum casemapping cm, const char *nick_old, const char *nick_new)
{
	/* Replace a user by name, maintaining modes */

	struct user *old, *new;

	old = user_list_get(ul, cm, nick_old, 0);
	new = user_list_get(ul, cm, nick_new, 0);

	if (old == NULL)
		return USER_ERR_NOT_FOUND;

	/* allow nick to change case  */
	if (new != NULL && irc_strcmp(cm, old->nick, new->nick))
		return USER_ERR_DUPLICATE;

	new = user(nick_new, old->prfxmodes);

	AVL_DEL(user_list, ul, old, &cm);
	AVL_ADD(user_list, ul, new, &cm);

	user_free(old);

	return USER_ERR_NONE;
}

struct user*
user_list_get(struct user_list *ul, enum casemapping cm, const char *nick, size_t prefix_len)
{
	struct user u = { .nick = nick };

	return AVL_GET(user_list, ul, &u, &cm, prefix_len);
}

void
user_list_free(struct user_list *ul)
{
	AVL_FOREACH(user_list, ul, user_free);

	memset(ul, 0, sizeof(*ul));
}

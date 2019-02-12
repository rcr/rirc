#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "src/components/user.h"
#include "src/utils/utils.h"

static struct user* user(const char*);

static inline int user_cmp(struct user*, struct user*);
static inline int user_ncmp(struct user*, struct user*, size_t);

static inline void user_free(struct user*);

AVL_GENERATE(user_list, user, ul, user_cmp, user_ncmp)

static inline int
user_cmp(struct user *u1, struct user *u2)
{
	/* TODO: CASEMAPPING */
	return irc_strcmp(CASEMAPPING_RFC1459, u1->nick, u2->nick);
}

static inline int
user_ncmp(struct user *u1, struct user *u2, size_t n)
{
	/* TODO: CASEMAPPING */
	return irc_strncmp(CASEMAPPING_RFC1459, u1->nick, u2->nick, n);
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

	size_t len = strlen(nick);

	if ((u = calloc(1, sizeof(*u) + len + 1)) == NULL)
		fatal("calloc: %s", strerror(errno));

	u->nick_len = len;
	u->nick = memcpy(u->_, nick, len + 1);

	return u;
}

enum user_err
user_list_add(struct user_list *ul, const char *nick, struct mode prfxmodes)
{
	/* Create user and add to userlist */

	struct user *u;

	if (user_list_get(ul, nick, 0) != NULL)
		return USER_ERR_DUPLICATE;

	u = AVL_ADD(user_list, ul, user(nick));
	ul->count++;

	u->prfxmodes = prfxmodes;

	return USER_ERR_NONE;
}

enum user_err
user_list_del(struct user_list *ul, const char *nick)
{
	/* Delete user and remove from userlist */

	struct user *u;

	if ((u = user_list_get(ul, nick, 0)) == NULL)
		return USER_ERR_NOT_FOUND;

	AVL_DEL(user_list, ul, u);
	ul->count--;

	user_free(u);

	return USER_ERR_NONE;
}

enum user_err
user_list_rpl(struct user_list *ul, const char *nick_old, const char *nick_new)
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

	new->prfxmodes = old->prfxmodes;

	user_free(old);

	return USER_ERR_NONE;
}

struct user*
user_list_get(struct user_list *ul, const char *nick, size_t prefix_len)
{
	struct user u2 = { .nick = nick };

	if (prefix_len == 0)
		return AVL_GET(user_list, ul, &u2);
	else
		return AVL_NGET(user_list, ul, &u2, prefix_len);
}

void
user_list_free(struct user_list *ul)
{
	AVL_FOREACH(user_list, ul, user_free);

	memset(ul, 0, sizeof(*ul));
}

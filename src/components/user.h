#ifndef RIRC_COMPONENTS_USER_H
#define RIRC_COMPONENTS_USER_H

#include "src/components/mode.h"
#include "src/utils/tree.h"
#include "src/utils/utils.h"

enum user_err
{
	USER_ERR_DUPLICATE = -2,
	USER_ERR_NOT_FOUND = -1,
	USER_ERR_NONE
};

struct user
{
	TREE_NODE(user) ul;
	const char *nick;
	size_t nick_len;
	struct mode prfxmodes;
	char _[];
};

struct user_list
{
	TREE_HEAD(user);
	unsigned count;
};

enum user_err user_list_add(struct user_list*, enum casemapping, const char*, struct mode);
enum user_err user_list_del(struct user_list*, enum casemapping, const char*);
enum user_err user_list_rpl(struct user_list*, enum casemapping, const char*, const char*);
struct user* user_list_get(struct user_list*, enum casemapping, const char*, size_t);
void user_list_free(struct user_list*);

#endif

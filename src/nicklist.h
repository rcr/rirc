#ifndef NICKLIST_H
#define NICKLIST_H

#include "avl.h"

struct nicklist
{
	struct avl_node *root;

	unsigned int count;
};

struct nick
{
	struct avl_node n;

	size_t len;
	char nick[];
};

int nicklist_add(struct nicklist*, char*);
int nicklist_del(struct nicklist*, char*);

struct nick* nicklist_get(struct nicklist*, char*, size_t);

void nicklist_free(struct nicklist*);

#endif

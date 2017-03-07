#ifndef NICKLIST_H
#define NICKLIST_H

typedef struct nicklist avl_tree

struct nick
{
	struct avl_node n;
	size_t len;
	char nick[];
}

int nicklist_add(struct nicklist*, char*);
int nicklist_del(struct nicklist*, char*);

struct nick nicklist_get(struct nicklist*, char*, size_t);

#endif

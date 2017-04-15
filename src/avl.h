#ifndef AVL_H
#define AVL_H

#include <stddef.h>

/* AVL tree node */
struct avl_node
{
	int height;
	struct avl_node *l;
	struct avl_node *r;
	char *key;
	void *val;
};

const struct avl_node* avl_get(struct avl_node*, const char*, int(*)(const char*, const char*, size_t), size_t);
int avl_add(struct avl_node**, const char*, int(*)(const char*, const char*), void*);
int avl_del(struct avl_node**, const char*, int(*)(const char*, const char*));
void free_avl(struct avl_node*);

#endif

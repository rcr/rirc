#ifndef AVL_H
#define AVL_H

#include <string.h>

/* AVL tree node */
typedef struct avl_node
{
	int height;
	struct avl_node *l;
	struct avl_node *r;
	char *key;
	void *val;
} avl_node;

const avl_node* avl_get(avl_node*, const char*, size_t);
int avl_add(avl_node**, const char*, void*);
int avl_del(avl_node**, const char*);
void free_avl(avl_node*);

#endif

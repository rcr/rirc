#ifndef AVL_H
#define AVL_H

struct avl_tree
{
	struct avl_node *root;
	unsigned int count;
};

struct avl_node
{
	int height;
	struct avl_node *l;
	struct avl_node *r;
};

void* avl_add(struct avl_tree*, void*, int (*)(void*, void*), size_t);
void* avl_del(struct avl_tree*, void*, int (*)(void*, void*));
void* avl_get(struct avl_tree*, void*, int (*)(void*, void*));

void avl_tree_free(struct avl_tree*, void (*)(void*));

#endif

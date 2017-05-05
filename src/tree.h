#ifndef TREE_H
#define TREE_H

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

/* Splay Tree */

#define SPLAY_LEFT(elm, field)  (elm)->field.splay_left
#define SPLAY_RIGHT(elm, field) (elm)->field.splay_right
#define SPLAY_ROOT(head)        (head)->splay_root
#define SPLAY_EMPTY(head)       (SPLAY_ROOT(head) == NULL)

#define SPLAY_INSERT(name, x, y) name##_SPLAY_INSERT(x, y)
#define SPLAY_REMOVE(name, x, y) name##_SPLAY_REMOVE(x, y)
#define SPLAY_FIND(name, x, y)   name##_SPLAY_FIND(x, y)
#define SPLAY_NEXT(name, x, y)   name##_SPLAY_NEXT(x, y)
#define SPLAY_PREV(name, x, y)   name##_SPLAY_PREV(x, y)


#define SPLAY_HEAD(type) \
    struct type *splay_root


#define SPLAY_NODE(type)          \
    struct {                      \
        struct type *splay_left;  \
        struct type *splay_right; \
    }


#define SPLAY_INIT(root) do {      \
        (root)->splay_root = NULL; \
    } while (0)                    \


#define SPLAY_ROTATE_RIGHT(head, tmp, field) do {                        \
        SPLAY_LEFT((head)->splay_root, field) = SPLAY_RIGHT(tmp, field); \
        SPLAY_RIGHT(tmp, field) = (head)->splay_root;                    \
        (head)->splay_root = tmp;                                        \
    } while (0)


#define SPLAY_ROTATE_LEFT(head, tmp, field) do {                         \
        SPLAY_RIGHT((head)->splay_root, field) = SPLAY_LEFT(tmp, field); \
        SPLAY_LEFT(tmp, field) = (head)->splay_root;                     \
        (head)->splay_root = tmp;                                        \
    } while (0)


#define SPLAY_LINK_RIGHT(head, tmp, field) do {                      \
        SPLAY_RIGHT(tmp, field) = (head)->splay_root;                \
        tmp = (head)->splay_root;                                    \
        (head)->splay_root = SPLAY_RIGHT((head)->splay_root, field); \
    } while (0)


#define SPLAY_LINK_LEFT(head, tmp, field) do {                      \
        SPLAY_LEFT(tmp, field) = (head)->splay_root;                \
        tmp = (head)->splay_root;                                   \
        (head)->splay_root = SPLAY_LEFT((head)->splay_root, field); \
    } while (0)


#define SPLAY_ASSEMBLE(head, node, left, right, field) do {                \
        SPLAY_RIGHT(left, field) = SPLAY_LEFT((head)->splay_root, field);  \
        SPLAY_LEFT(right, field) = SPLAY_RIGHT((head)->splay_root, field); \
        SPLAY_LEFT((head)->splay_root, field) = SPLAY_RIGHT(node, field);  \
        SPLAY_RIGHT((head)->splay_root, field) = SPLAY_LEFT(node, field);  \
    } while (0)


#define SPLAY_GENERATE(name, type, field, cmp)                                \
    struct type *name##_SPLAY_INSERT(struct name *, struct type *);           \
    struct type *name##_SPLAY_REMOVE(struct name *, struct type *);           \
    void name##_SPLAY(struct name *, struct type *);                          \
                                                                              \
static inline struct type *                                                   \
name##_SPLAY_FIND(struct name *head, struct type *elm)                        \
{                                                                             \
    if (SPLAY_EMPTY(head))                                                    \
        return(NULL);                                                         \
                                                                              \
    name##_SPLAY(head, elm);                                                  \
                                                                              \
    if ((cmp)(elm, (head)->splay_root) == 0)                                  \
        return (head->splay_root);                                            \
                                                                              \
    return (NULL);                                                            \
}                                                                             \
                                                                              \
static inline struct type *                                                   \
name##_SPLAY_NEXT(struct name *head, struct type *elm)                        \
{                                                                             \
    name##_SPLAY(head, elm);                                                  \
                                                                              \
    if (SPLAY_RIGHT(elm, field) != NULL) {                                    \
        elm = SPLAY_RIGHT(elm, field);                                        \
        while (SPLAY_LEFT(elm, field) != NULL) {                              \
            elm = SPLAY_LEFT(elm, field);                                     \
        }                                                                     \
    } else                                                                    \
        elm = NULL;                                                           \
                                                                              \
    return (elm);                                                             \
}                                                                             \
                                                                              \
static inline struct type *                                                   \
name##_SPLAY_PREV(struct name *head, struct type *elm)                        \
{                                                                             \
    /* TODO */                                                                \
    (void)((head));                                                           \
    (void)((elm));                                                            \
    return NULL;                                                              \
}                                                                             \
                                                                              \
struct type *                                                                 \
name##_SPLAY_INSERT(struct name *head, struct type *elm)                      \
{                                                                             \
    if (SPLAY_EMPTY(head)) {                                                  \
        SPLAY_LEFT(elm, field) = SPLAY_RIGHT(elm, field) = NULL;              \
    } else {                                                                  \
        int comp;                                                             \
        name##_SPLAY(head, elm);                                              \
        comp = (cmp)(elm, (head)->splay_root);                                \
        if(comp < 0) {                                                        \
            SPLAY_LEFT(elm, field) = SPLAY_LEFT((head)->splay_root, field);   \
            SPLAY_RIGHT(elm, field) = (head)->splay_root;                     \
            SPLAY_LEFT((head)->splay_root, field) = NULL;                     \
        } else if (comp > 0) {                                                \
            SPLAY_RIGHT(elm, field) = SPLAY_RIGHT((head)->splay_root, field); \
            SPLAY_LEFT(elm, field) = (head)->splay_root;                      \
            SPLAY_RIGHT((head)->splay_root, field) = NULL;                    \
        } else                                                                \
            return ((head)->splay_root);                                      \
    }                                                                         \
    (head)->splay_root = (elm);                                               \
    return (NULL);                                                            \
}                                                                             \
                                                                              \
struct type *                                                                 \
name##_SPLAY_REMOVE(struct name *head, struct type *elm)                      \
{                                                                             \
    struct type *tmp;                                                         \
    if (SPLAY_EMPTY(head))                                                    \
        return (NULL);                                                        \
    name##_SPLAY(head, elm);                                                  \
    if ((cmp)(elm, (head)->splay_root) == 0) {                                \
        if (SPLAY_LEFT((head)->splay_root, field) == NULL) {                  \
            (head)->splay_root = SPLAY_RIGHT((head)->splay_root, field);      \
        } else {                                                              \
            tmp = SPLAY_RIGHT((head)->splay_root, field);                     \
            (head)->splay_root = SPLAY_LEFT((head)->splay_root, field);       \
            name##_SPLAY(head, elm);                                          \
            SPLAY_RIGHT((head)->splay_root, field) = tmp;                     \
        }                                                                     \
        return (elm);                                                         \
    }                                                                         \
    return (NULL);                                                            \
}                                                                             \
                                                                              \
void                                                                          \
name##_SPLAY(struct name *head, struct type *elm)                             \
{                                                                             \
    struct type node, *left, *right, *tmp;                                    \
    int comp;                                                                 \
                                                                              \
    SPLAY_LEFT(&node, field) = SPLAY_RIGHT(&node, field) = NULL;              \
    left = right = &node;                                                     \
                                                                              \
    while ((comp = (cmp)(elm, (head)->splay_root)) != 0) {                    \
        if (comp < 0) {                                                       \
            tmp = SPLAY_LEFT((head)->splay_root, field);                      \
            if (tmp == NULL)                                                  \
                break;                                                        \
            if ((cmp)(elm, tmp) < 0){                                         \
                SPLAY_ROTATE_RIGHT(head, tmp, field);                         \
                if (SPLAY_LEFT((head)->splay_root, field) == NULL)            \
                    break;                                                    \
            }                                                                 \
            SPLAY_LINK_LEFT(head, right, field);                              \
        } else if (comp > 0) {                                                \
            tmp = SPLAY_RIGHT((head)->splay_root, field);                     \
            if (tmp == NULL)                                                  \
                break;                                                        \
            if ((cmp)(elm, tmp) > 0){                                         \
                SPLAY_ROTATE_LEFT(head, tmp, field);                          \
                if (SPLAY_RIGHT((head)->splay_root, field) == NULL)           \
                    break;                                                    \
            }                                                                 \
            SPLAY_LINK_RIGHT(head, left, field);                              \
        }                                                                     \
    }                                                                         \
    SPLAY_ASSEMBLE(head, &node, left, right, field);                          \
}

#endif

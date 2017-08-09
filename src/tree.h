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

#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define TREE_EMPTY(head)       (TREE_ROOT(head) == NULL)
#define TREE_LEFT(elm, field)  (elm)->field.tree_left
#define TREE_RIGHT(elm, field) (elm)->field.tree_right
#define TREE_ROOT(head)        (head)->tree_root

/* AVL Tree */

#define AVL_HEIGHT(elm, field) (elm)->field.height

#define AVL_NEW(name, x, y) name##_AVL_NEW(x, y)
#define AVL_ADD(name, x, y) name##_AVL_ADD(x, y)
#define AVL_DEL(name, x, y) name##_AVL_DEL(x, y)
#define AVL_GET(name, x, y) name##_AVL_GET(x, y)


#define AVL_HEAD(type) \
    struct type *tree_root


#define AVL_NODE(type)           \
    struct {                     \
        int height;              \
        struct type *tree_left;  \
        struct type *tree_right; \
    }


#define AVL_GENERATE(name, type, field, cmp)                                      \
    struct type* name##_AVL_ADD(struct name*, struct type*);                      \
    struct type* name##_AVL_DEL(struct name*, struct type*);                      \
    struct type* name##_AVL_ADD_REC(struct type*, struct type*);                  \
                                                                                  \
static inline void                                                                \
name##_AVL_INIT(struct type *elm)                                                 \
{                                                                                 \
    AVL_HEIGHT(elm, field) = 1;                                                   \
    TREE_RIGHT(elm, field) = TREE_LEFT(elm, field) = NULL;                        \
}                                                                                 \
                                                                                  \
static inline int                                                                 \
name##_AVL_GET_HEIGHT(struct type *elm)                                           \
{                                                                                 \
    return (elm == NULL) ? 0 : AVL_HEIGHT(elm, field);                            \
}                                                                                 \
                                                                                  \
static inline int                                                                 \
name##_AVL_SET_HEIGHT(struct type *elm)                                           \
{                                                                                 \
    int hL = name##_AVL_GET_HEIGHT(TREE_LEFT(elm, field)),                        \
        hR = name##_AVL_GET_HEIGHT(TREE_RIGHT(elm, field));                       \
                                                                                  \
    return (AVL_HEIGHT(elm, field) = 1 + MAX(hL, hR));                            \
}                                                                                 \
                                                                                  \
static inline int                                                                 \
name##_AVL_BALANCE(struct type *elm)                                              \
{                                                                                 \
    int hL = name##_AVL_GET_HEIGHT(TREE_LEFT(elm, field)),                        \
        hR = name##_AVL_GET_HEIGHT(TREE_RIGHT(elm, field));                       \
                                                                                  \
    return (hR - hL);                                                             \
}                                                                                 \
                                                                                  \
static inline struct type*                                                        \
name##_AVL_ROTATE_LEFT(struct type *n)                                            \
{                                                                                 \
    struct type *p = TREE_RIGHT(n, field);                                        \
    struct type *b = TREE_LEFT(p, field);                                         \
                                                                                  \
    TREE_LEFT(p, field) = n;                                                      \
    TREE_RIGHT(n, field) = b;                                                     \
                                                                                  \
    name##_AVL_SET_HEIGHT(n);                                                     \
    name##_AVL_SET_HEIGHT(p);                                                     \
                                                                                  \
    return p;                                                                     \
}                                                                                 \
                                                                                  \
static inline struct type*                                                        \
name##_AVL_ROTATE_RIGHT(struct type *n)                                           \
{                                                                                 \
    struct type *p = TREE_LEFT(n, field);                                         \
    struct type *b = TREE_RIGHT(p, field);                                        \
                                                                                  \
    TREE_RIGHT(p, field) = n;                                                     \
    TREE_LEFT(n, field) = b;                                                      \
                                                                                  \
    name##_AVL_SET_HEIGHT(n);                                                     \
    name##_AVL_SET_HEIGHT(p);                                                     \
                                                                                  \
    return p;                                                                     \
}                                                                                 \
                                                                                  \
static inline struct type*                                                        \
name##_AVL_GET(struct name *head, struct type *elm)                               \
{                                                                                 \
    struct type *tmp = TREE_ROOT(head);                                           \
                                                                                  \
    int comp;                                                                     \
                                                                                  \
    while (tmp && (comp = (cmp)(elm, tmp)))                                       \
        tmp = (comp > 0) ? TREE_RIGHT(tmp, field) : TREE_LEFT(tmp, field);        \
                                                                                  \
    return tmp;                                                                   \
}                                                                                 \
                                                                                  \
struct type*                                                                      \
name##_AVL_ADD(struct name *head, struct type *elm)                               \
{                                                                                 \
    name##_AVL_INIT(elm);                                                         \
                                                                                  \
    struct type *r = name##_AVL_ADD_REC(TREE_ROOT(head), elm);                    \
                                                                                  \
    if (r == NULL)                                                                \
        return NULL;                                                              \
                                                                                  \
    TREE_ROOT(head) = r;                                                          \
                                                                                  \
    return elm;                                                                   \
}                                                                                 \
                                                                                  \
struct type*                                                                      \
name##_AVL_ADD_REC(struct type *n, struct type *elm)                              \
{                                                                                 \
    struct type *tmp;                                                             \
                                                                                  \
    int comp, balance;                                                            \
                                                                                  \
    if (n == NULL)                                                                \
        return elm;                                                               \
                                                                                  \
    if ((comp = (cmp)(elm, n)) == 0)                                              \
        return NULL;                                                              \
                                                                                  \
    else if (comp > 0) {                                                          \
                                                                                  \
        if ((tmp = name##_AVL_ADD_REC(TREE_RIGHT(n, field), elm)) == NULL)        \
            return NULL;                                                          \
                                                                                  \
        TREE_RIGHT(n, field) = tmp;                                               \
    }                                                                             \
                                                                                  \
    else if (comp < 0) {                                                          \
                                                                                  \
        if ((tmp = name##_AVL_ADD_REC(TREE_LEFT(n, field), elm)) == NULL)         \
            return NULL;                                                          \
                                                                                  \
        TREE_LEFT(n, field) = tmp;                                                \
    }                                                                             \
                                                                                  \
    name##_AVL_SET_HEIGHT(n);                                                     \
                                                                                  \
    balance = name##_AVL_BALANCE(n);                                              \
                                                                                  \
    if (balance > 1) {                                                            \
                                                                                  \
        if (((cmp)(elm, TREE_RIGHT(n, field))) < 0)                               \
            TREE_RIGHT(n, field) = name##_AVL_ROTATE_RIGHT(TREE_RIGHT(n, field)); \
                                                                                  \
        return name##_AVL_ROTATE_LEFT(n);                                         \
    }                                                                             \
                                                                                  \
    if (balance < -1) {                                                           \
                                                                                  \
        if (((cmp)(elm, TREE_LEFT(n, field))) > 0)                                \
            TREE_LEFT(n, field) = name##_AVL_ROTATE_LEFT(TREE_LEFT(n, field));    \
                                                                                  \
        return name##_AVL_ROTATE_RIGHT(n);                                        \
    }                                                                             \
                                                                                  \
    return n;                                                                     \
}                                                                                 \
                                                                                  \
struct type*                                                                      \
name##_AVL_DEL(struct name *head, struct type *elm)                               \
{ (void)(head); (void)(elm); return NULL; }                                       \



/* Splay Tree */

#define SPLAY_ADD(name, x, y)  name##_SPLAY_ADD(x, y)
#define SPLAY_DEL(name, x, y)  name##_SPLAY_DEL(x, y)
#define SPLAY_GET(name, x, y)  name##_SPLAY_GET(x, y)
#define SPLAY_MAX(name, x)     (TREE_EMPTY(x) ? NULL : name##_SPLAY_MAX(x))
#define SPLAY_MIN(name, x)     (TREE_EMPTY(x) ? NULL : name##_SPLAY_MIN(x))
#define SPLAY_NEXT(name, x, y) name##_SPLAY_NEXT(x, y)
#define SPLAY_PREV(name, x, y) name##_SPLAY_PREV(x, y)


#define SPLAY_HEAD(type) \
    struct type *tree_root


#define SPLAY_NODE(type)         \
    struct {                     \
        struct type *tree_left;  \
        struct type *tree_right; \
    }


#define SPLAY_INIT(root) do {   \
        TREE_ROOT(root) = NULL; \
    } while (0)


#define SPLAY_ROTATE_RIGHT(head, tmp, field) do {                   \
        TREE_LEFT(TREE_ROOT(head), field) = TREE_RIGHT(tmp, field); \
        TREE_RIGHT(tmp, field) = TREE_ROOT(head);                   \
        TREE_ROOT(head) = tmp;                                      \
    } while (0)


#define SPLAY_ROTATE_LEFT(head, tmp, field) do {                    \
        TREE_RIGHT(TREE_ROOT(head), field) = TREE_LEFT(tmp, field); \
        TREE_LEFT(tmp, field) = TREE_ROOT(head);                    \
        TREE_ROOT(head) = tmp;                                      \
    } while (0)


#define SPLAY_LINK_RIGHT(head, tmp, field) do {               \
        TREE_RIGHT(tmp, field) = TREE_ROOT(head);             \
        tmp = TREE_ROOT(head);                                \
        TREE_ROOT(head) = TREE_RIGHT(TREE_ROOT(head), field); \
    } while (0)


#define SPLAY_LINK_LEFT(head, tmp, field) do {               \
        TREE_LEFT(tmp, field) = TREE_ROOT(head);             \
        tmp = TREE_ROOT(head);                               \
        TREE_ROOT(head) = TREE_LEFT(TREE_ROOT(head), field); \
    } while (0)


#define SPLAY_ASSEMBLE(head, node, left, right, field) do {           \
        TREE_RIGHT(left, field) = TREE_LEFT(TREE_ROOT(head), field);  \
        TREE_LEFT(right, field) = TREE_RIGHT(TREE_ROOT(head), field); \
        TREE_LEFT(TREE_ROOT(head), field) = TREE_RIGHT(node, field);  \
        TREE_RIGHT(TREE_ROOT(head), field) = TREE_LEFT(node, field);  \
    } while (0)


#define SPLAY_FOREACH(x, name, head)      \
    for ((x) = SPLAY_MIN(name, head);     \
         (x) != NULL;                     \
         (x) = SPLAY_NEXT(name, head, x))


#define SPLAY_GENERATE(name, type, field, cmp)                           \
    struct type* name##_SPLAY_ADD(struct name*, struct type*);           \
    struct type* name##_SPLAY_DEL(struct name*, struct type*);           \
    void name##_SPLAY(struct name*, struct type*);                       \
                                                                         \
static inline struct type*                                               \
name##_SPLAY_MIN(struct name *head)                                      \
{                                                                        \
    struct type *x = TREE_ROOT(head);                                    \
                                                                         \
    while (TREE_LEFT(x, field) != NULL)                                  \
        x = TREE_LEFT(x, field);                                         \
                                                                         \
    return x;                                                            \
}                                                                        \
                                                                         \
static inline struct type*                                               \
name##_SPLAY_MAX(struct name *head)                                      \
{                                                                        \
    struct type *x = TREE_ROOT(head);                                    \
                                                                         \
    while (TREE_RIGHT(x, field) != NULL)                                 \
        x = TREE_RIGHT(x, field);                                        \
                                                                         \
    return x;                                                            \
}                                                                        \
                                                                         \
static inline struct type*                                               \
name##_SPLAY_NEXT(struct name *head, struct type *elm)                   \
{                                                                        \
    name##_SPLAY(head, elm);                                             \
                                                                         \
    if (TREE_RIGHT(elm, field) == NULL)                                  \
        return NULL;                                                     \
                                                                         \
    elm = TREE_RIGHT(elm, field);                                        \
                                                                         \
    while (TREE_LEFT(elm, field) != NULL)                                \
        elm = TREE_LEFT(elm, field);                                     \
                                                                         \
    return elm;                                                          \
}                                                                        \
                                                                         \
static inline struct type*                                               \
name##_SPLAY_PREV(struct name *head, struct type *elm)                   \
{                                                                        \
    name##_SPLAY(head, elm);                                             \
                                                                         \
    if (TREE_LEFT(elm, field) == NULL)                                   \
        return NULL;                                                     \
                                                                         \
    elm = TREE_LEFT(elm, field);                                         \
                                                                         \
    while (TREE_RIGHT(elm, field) != NULL)                               \
        elm = TREE_RIGHT(elm, field);                                    \
                                                                         \
    return elm;                                                          \
}                                                                        \
                                                                         \
static inline struct type*                                               \
name##_SPLAY_GET(struct name *head, struct type *elm)                    \
{                                                                        \
    if (TREE_EMPTY(head))                                                \
        return NULL;                                                     \
                                                                         \
    name##_SPLAY(head, elm);                                             \
                                                                         \
    if ((cmp)(elm, TREE_ROOT(head)) == 0)                                \
        return TREE_ROOT(head);                                          \
                                                                         \
    return NULL;                                                         \
}                                                                        \
                                                                         \
struct type*                                                             \
name##_SPLAY_ADD(struct name *head, struct type *elm)                    \
{                                                                        \
    if (TREE_EMPTY(head)) {                                              \
        TREE_LEFT(elm, field) = TREE_RIGHT(elm, field) = NULL;           \
    } else {                                                             \
        int comp;                                                        \
        name##_SPLAY(head, elm);                                         \
        comp = (cmp)(elm, TREE_ROOT(head));                              \
        if (comp < 0) {                                                  \
            TREE_LEFT(elm, field) = TREE_LEFT(TREE_ROOT(head), field);   \
            TREE_RIGHT(elm, field) = TREE_ROOT(head);                    \
            TREE_LEFT(TREE_ROOT(head), field) = NULL;                    \
        } else if (comp > 0) {                                           \
            TREE_RIGHT(elm, field) = TREE_RIGHT(TREE_ROOT(head), field); \
            TREE_LEFT(elm, field) = TREE_ROOT(head);                     \
            TREE_RIGHT(TREE_ROOT(head), field) = NULL;                   \
        } else                                                           \
            return (TREE_ROOT(head));                                    \
    }                                                                    \
    TREE_ROOT(head) = (elm);                                             \
    return NULL;                                                         \
}                                                                        \
                                                                         \
struct type*                                                             \
name##_SPLAY_DEL(struct name *head, struct type *elm)                    \
{                                                                        \
    struct type *tmp;                                                    \
    if (TREE_EMPTY(head))                                                \
        return NULL;                                                     \
    name##_SPLAY(head, elm);                                             \
    if ((cmp)(elm, TREE_ROOT(head)) == 0) {                              \
        if (TREE_LEFT(TREE_ROOT(head), field) == NULL) {                 \
            TREE_ROOT(head) = TREE_RIGHT(TREE_ROOT(head), field);        \
        } else {                                                         \
            tmp = TREE_RIGHT(TREE_ROOT(head), field);                    \
            TREE_ROOT(head) = TREE_LEFT(TREE_ROOT(head), field);         \
            name##_SPLAY(head, elm);                                     \
            TREE_RIGHT(TREE_ROOT(head), field) = tmp;                    \
        }                                                                \
        return (elm);                                                    \
    }                                                                    \
    return NULL;                                                         \
}                                                                        \
                                                                         \
void                                                                     \
name##_SPLAY(struct name *head, struct type *elm)                        \
{                                                                        \
    struct type node, *left, *right, *tmp;                               \
    int comp;                                                            \
                                                                         \
    TREE_LEFT(&node, field) = TREE_RIGHT(&node, field) = NULL;           \
    left = right = &node;                                                \
                                                                         \
    while ((comp = (cmp)(elm, TREE_ROOT(head))) != 0) {                  \
        if (comp < 0) {                                                  \
            tmp = TREE_LEFT(TREE_ROOT(head), field);                     \
            if (tmp == NULL)                                             \
                break;                                                   \
            if ((cmp)(elm, tmp) < 0){                                    \
                SPLAY_ROTATE_RIGHT(head, tmp, field);                    \
                if (TREE_LEFT(TREE_ROOT(head), field) == NULL)           \
                    break;                                               \
            }                                                            \
            SPLAY_LINK_LEFT(head, right, field);                         \
        } else if (comp > 0) {                                           \
            tmp = TREE_RIGHT(TREE_ROOT(head), field);                    \
            if (tmp == NULL)                                             \
                break;                                                   \
            if ((cmp)(elm, tmp) > 0){                                    \
                SPLAY_ROTATE_LEFT(head, tmp, field);                     \
                if (TREE_RIGHT(TREE_ROOT(head), field) == NULL)          \
                    break;                                               \
            }                                                            \
            SPLAY_LINK_RIGHT(head, left, field);                         \
        }                                                                \
    }                                                                    \
    SPLAY_ASSEMBLE(head, &node, left, right, field);                     \
}                                                                        \
                                                                         \
/* Suppress unused function warnings */                                  \
void (*name##_SPLAY_DUMMY[])(void) = {                                   \
    (void(*)(void))(name##_SPLAY_MAX),                                   \
    (void(*)(void))(name##_SPLAY_MIN),                                   \
    (void(*)(void))(name##_SPLAY_NEXT),                                  \
    (void(*)(void))(name##_SPLAY_PREV) };

#endif

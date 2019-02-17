#ifndef TREE_H
#define TREE_H

/* FIXME: undefined behaviour reported by clang in AVL/SPLAY implementations */

#include <stddef.h>

#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define TREE_EMPTY(head)       (TREE_ROOT(head) == NULL)
#define TREE_LEFT(elm, field)  (elm)->field.tree_left
#define TREE_RIGHT(elm, field) (elm)->field.tree_right
#define TREE_ROOT(head)        (head)->tree_root

/* AVL Tree */

#define AVL_HEIGHT(elm, field) (elm)->field.height

#define AVL_ADD(name, x, y, z)     name##_AVL_ADD(x, y, z)
#define AVL_DEL(name, x, y, z)     name##_AVL_DEL(x, y, z)
#define AVL_GET(name, x, y, z)     name##_AVL_GET(x, y, z)
#define AVL_NGET(name, x, y, z, n) name##_AVL_NGET(x, y, z, n)

#define AVL_FOREACH(name, x, y) name##_AVL_FOREACH(x, y)

#define AVL_HEAD(type) \
    struct type *tree_root


#define AVL_NODE(type)           \
    struct {                     \
        int height;              \
        struct type *tree_left;  \
        struct type *tree_right; \
    }

/* FIXME: scan-build showing NULL pointer dereferences on add */

#define AVL_GENERATE(name, type, field, cmp, ncmp) \
    static struct type* name##_AVL_ADD(struct name*, struct type*, void*);        \
    static struct type* name##_AVL_DEL(struct name*, struct type*, void*);        \
    static struct type* name##_AVL_ADD_REC(struct type*, struct type*, void*);    \
    static struct type* name##_AVL_DEL_REC(struct type**, struct type*, void*);   \
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
    name##_AVL_SET_HEIGHT(elm);                                                   \
                                                                                  \
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
static void                                                                       \
name##_AVL_FOREACH_REC(struct type *elm, void (*f)(struct type*))                 \
{                                                                                 \
    if (elm) {                                                                    \
        name##_AVL_FOREACH_REC(TREE_LEFT(elm, field), f);                         \
        name##_AVL_FOREACH_REC(TREE_RIGHT(elm, field), f);                        \
        f(elm);                                                                   \
    }                                                                             \
}                                                                                 \
                                                                                  \
static inline void                                                                \
name##_AVL_FOREACH(struct name *head, void (*f)(struct type*))                    \
{                                                                                 \
    name##_AVL_FOREACH_REC(TREE_ROOT(head), f);                                   \
}                                                                                 \
                                                                                  \
static struct type*                                                               \
name##_AVL_GET(struct name *head, struct type *elm, void *arg)                    \
{                                                                                 \
    int comp;                                                                     \
    struct type *tmp = TREE_ROOT(head);                                           \
                                                                                  \
    while (tmp && (comp = cmp(elm, tmp, arg)))                                    \
        tmp = (comp > 0) ? TREE_RIGHT(tmp, field) : TREE_LEFT(tmp, field);        \
                                                                                  \
    return tmp;                                                                   \
}                                                                                 \
                                                                                  \
static struct type*                                                               \
name##_AVL_NGET(struct name *head, struct type *elm, void *arg, size_t n)         \
{                                                                                 \
    int comp;                                                                     \
    struct type *tmp = TREE_ROOT(head);                                           \
                                                                                  \
    while (tmp && (comp = ncmp(elm, tmp, arg, n)))                                \
        tmp = (comp > 0) ? TREE_RIGHT(tmp, field) : TREE_LEFT(tmp, field);        \
                                                                                  \
    return tmp;                                                                   \
}                                                                                 \
                                                                                  \
static struct type*                                                               \
name##_AVL_ADD(struct name *head, struct type *elm, void *arg)                    \
{                                                                                 \
    name##_AVL_INIT(elm);                                                         \
                                                                                  \
    struct type *r = name##_AVL_ADD_REC(TREE_ROOT(head), elm, arg);               \
                                                                                  \
    if (r == NULL)                                                                \
        return NULL;                                                              \
                                                                                  \
    TREE_ROOT(head) = r;                                                          \
                                                                                  \
    return elm;                                                                   \
}                                                                                 \
                                                                                  \
static struct type*                                                               \
name##_AVL_ADD_REC(struct type *n, struct type *elm, void *arg)                   \
{                                                                                 \
    int comp, balance;                                                            \
    struct type *tmp;                                                             \
                                                                                  \
    if (n == NULL)                                                                \
        return elm;                                                               \
                                                                                  \
    if ((comp = cmp(elm, n, arg)) == 0) {                                         \
        return NULL;                                                              \
    } else if (comp > 0) {                                                        \
                                                                                  \
        if ((tmp = name##_AVL_ADD_REC(TREE_RIGHT(n, field), elm, arg)) == NULL)   \
            return NULL;                                                          \
                                                                                  \
        TREE_RIGHT(n, field) = tmp;                                               \
                                                                                  \
    } else if (comp < 0) {                                                        \
                                                                                  \
        if ((tmp = name##_AVL_ADD_REC(TREE_LEFT(n, field), elm, arg)) == NULL)    \
            return NULL;                                                          \
                                                                                  \
        TREE_LEFT(n, field) = tmp;                                                \
    }                                                                             \
                                                                                  \
    balance = name##_AVL_BALANCE(n);                                              \
                                                                                  \
    if (balance > 1) {                                                            \
                                                                                  \
        if ((cmp(elm, TREE_RIGHT(n, field), arg)) < 0)                            \
            TREE_RIGHT(n, field) = name##_AVL_ROTATE_RIGHT(TREE_RIGHT(n, field)); \
                                                                                  \
        return name##_AVL_ROTATE_LEFT(n);                                         \
    }                                                                             \
                                                                                  \
    if (balance < -1) {                                                           \
                                                                                  \
        if ((cmp(elm, TREE_LEFT(n, field), arg)) > 0)                             \
            TREE_LEFT(n, field) = name##_AVL_ROTATE_LEFT(TREE_LEFT(n, field));    \
                                                                                  \
        return name##_AVL_ROTATE_RIGHT(n);                                        \
    }                                                                             \
                                                                                  \
    return n;                                                                     \
}                                                                                 \
                                                                                  \
static struct type*                                                               \
name##_AVL_DEL(struct name *head, struct type *elm, void *arg)                    \
{                                                                                 \
    return name##_AVL_DEL_REC(&TREE_ROOT(head), elm, arg);                        \
}                                                                                 \
                                                                                  \
static struct type*                                                               \
name##_AVL_DEL_REC(struct type **p, struct type *elm, void *arg)                  \
{                                                                                 \
    int comp, balance;                                                            \
    struct type *n = *p;                                                          \
    struct type *ret;                                                             \
                                                                                  \
    if (n == NULL)                                                                \
        return NULL;                                                              \
                                                                                  \
    if ((comp = cmp(elm, n, arg)) == 0) {                                         \
                                                                                  \
        ret = n;                                                                  \
                                                                                  \
        if (TREE_LEFT(n, field) && TREE_RIGHT(n, field)) {                        \
                                                                                  \
            struct type *swap   = TREE_RIGHT(n, field),                           \
                        *swap_p = TREE_RIGHT(n, field);                           \
                                                                                  \
            while (TREE_LEFT(swap, field)) {                                      \
                swap_p = swap;                                                    \
                swap = TREE_LEFT(swap, field);                                    \
            }                                                                     \
                                                                                  \
            if (swap_p == swap) {                                                 \
                TREE_LEFT(swap, field) = TREE_LEFT(n, field);                     \
            } else {                                                              \
                struct type *swap_l = TREE_LEFT(swap, field),                     \
                            *swap_r = TREE_RIGHT(swap, field);                    \
                                                                                  \
                TREE_LEFT(swap, field) = TREE_LEFT(n, field);                     \
                TREE_RIGHT(swap, field) = TREE_RIGHT(n, field);                   \
                                                                                  \
                TREE_LEFT(n, field) = swap_l;                                     \
                TREE_RIGHT(n, field) = swap_r;                                    \
                                                                                  \
                TREE_LEFT(swap_p, field) = n;                                     \
                                                                                  \
                name##_AVL_DEL_REC(&TREE_RIGHT(swap, field), elm, arg);           \
            }                                                                     \
                                                                                  \
            *p = n = swap;                                                        \
        } else if (TREE_LEFT(n, field))  {                                        \
            *p = TREE_LEFT(n, field);                                             \
        } else if (TREE_RIGHT(n, field)) {                                        \
            *p = TREE_RIGHT(n, field);                                            \
        } else {                                                                  \
            *p = NULL;                                                            \
        }                                                                         \
    } else if (comp < 0) {                                                        \
        ret = name##_AVL_DEL_REC(&TREE_LEFT(n, field), elm, arg);                 \
    } else if (comp > 0) {                                                        \
        ret = name##_AVL_DEL_REC(&TREE_RIGHT(n, field), elm, arg);                \
    }                                                                             \
                                                                                  \
    if (ret == NULL)                                                              \
        return NULL;                                                              \
                                                                                  \
    balance = name##_AVL_BALANCE(n);                                              \
                                                                                  \
    if (balance > 1) {                                                            \
        int hrl = name##_AVL_GET_HEIGHT(TREE_LEFT(TREE_RIGHT(n, field), field)),  \
            hrr = name##_AVL_GET_HEIGHT(TREE_RIGHT(TREE_RIGHT(n, field), field)); \
                                                                                  \
        if ((hrl - hrr) > 0) {                                                    \
            TREE_RIGHT(n, field) = name##_AVL_ROTATE_RIGHT(TREE_RIGHT(n, field)); \
        }                                                                         \
                                                                                  \
        *p = name##_AVL_ROTATE_LEFT(n);                                           \
    }                                                                             \
                                                                                  \
    if (balance < -1) {                                                           \
        int hll = name##_AVL_GET_HEIGHT(TREE_LEFT(TREE_LEFT(n, field), field)),   \
            hlr = name##_AVL_GET_HEIGHT(TREE_RIGHT(TREE_LEFT(n, field), field));  \
                                                                                  \
        if ((hll - hlr) < 0) {                                                    \
            TREE_LEFT(n, field) = name##_AVL_ROTATE_LEFT(TREE_LEFT(n, field));    \
        }                                                                         \
                                                                                  \
        *p = name##_AVL_ROTATE_RIGHT(n);                                          \
    }                                                                             \
                                                                                  \
    return ret;                                                                   \
}

/* Splay Tree */

#define SPLAY_ADD(name, x, y)  name##_SPLAY_ADD(x, y)
#define SPLAY_DEL(name, x, y)  name##_SPLAY_DEL(x, y)
#define SPLAY_GET(name, x, y)  name##_SPLAY_GET(x, y)
#define SPLAY_MAX(name, x)     (TREE_EMPTY(x) ? NULL : name##_SPLAY_MAX(x))
#define SPLAY_MIN(name, x)     (TREE_EMPTY(x) ? NULL : name##_SPLAY_MIN(x))


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
name##_SPLAY_GET(struct name *head, struct type *elm)                    \
{                                                                        \
    if (TREE_EMPTY(head))                                                \
        return NULL;                                                     \
                                                                         \
    name##_SPLAY(head, elm);                                             \
                                                                         \
    if (cmp(elm, TREE_ROOT(head)) == 0)                                  \
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
        comp = cmp(elm, TREE_ROOT(head));                                \
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
    if (cmp(elm, TREE_ROOT(head)) == 0) {                                \
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
    while ((comp = cmp(elm, TREE_ROOT(head))) != 0) {                    \
        if (comp < 0) {                                                  \
            tmp = TREE_LEFT(TREE_ROOT(head), field);                     \
            if (tmp == NULL)                                             \
                break;                                                   \
            if (cmp(elm, tmp) < 0){                                      \
                SPLAY_ROTATE_RIGHT(head, tmp, field);                    \
                if (TREE_LEFT(TREE_ROOT(head), field) == NULL)           \
                    break;                                               \
            }                                                            \
            SPLAY_LINK_LEFT(head, right, field);                         \
        } else if (comp > 0) {                                           \
            tmp = TREE_RIGHT(TREE_ROOT(head), field);                    \
            if (tmp == NULL)                                             \
                break;                                                   \
            if (cmp(elm, tmp) > 0){                                      \
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
    (void(*)(void))(name##_SPLAY_MIN) };

#endif

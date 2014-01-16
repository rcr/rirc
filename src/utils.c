#include <stdlib.h>
#include <string.h>

/* AVL Tree implementation */
typedef struct node {
	char *nick;
	struct node *l;
	struct node *r;
} node;

typedef struct nicklist {
	int count;
	node *root;
} nicklist;

int nick_cmp(char*, char*);
void nicklist_search(char*, nicklist*);
void nicklist_insert(char*, nicklist*);
void nicklist_delete(char*, nicklist*);



int
nick_cmp(char *n1, char *n2)
{
	/* Don't need to check for end of string, the longer will
	 * compare a character to \0 and return */
	for (;;) {
		if (*n1 == *n2)
			n1++, n2++;
		else if (*n1 > *n2)
			return 1;
		else if (*n1 < *n2)
			return 0;
	};
}

void
insert_nick(char *nick, nicklist *list)
{
	node **n = &list->root;

	for (;;) {
		if (*n == NULL)
			break;
		else if (nick_cmp(nick, (*n)->nick))
			n = &(*n)->r;
		else
			n = &(*n)->l;
	}
	/* n should be our target to insert now */
	if ((*n = malloc(sizeof(node))) == NULL)
		return; //		fatal("insert_nick");
	if (((*n)->nick = malloc(strlen(nick))) == NULL)
		return; //		fatal("insert_nick");
	strcpy((*n)->nick, nick);
	(*n)->l = (*n)->r = NULL;
	list->count++;
}

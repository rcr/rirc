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

void nicklist_search(char*, nicklist*);
void nicklist_insert(char*, nicklist*);
void nicklist_delete(char*, nicklist*);



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

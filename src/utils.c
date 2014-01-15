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

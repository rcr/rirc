#define BUFFSIZE 512
#define MAXINPUT 200
#define SCROLLBACK 10

struct channel
{
	int active;
	int cur_line;
	char name[50];
	char *chat[SCROLLBACK];
};

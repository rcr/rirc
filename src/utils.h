#ifndef UTILS_H
#define UTILS_H

#include <errno.h>

//TODO: struct string { len, text[] } for
// fields often strlen'ed, e.g. usernames, channel names, server names
// strcmp comparing len == len && strlen

/* Parsed IRC message */
struct parsed_mesg
{
	char *from;
	char *host;
	char *command;
	char *params; /* TODO: char*[15] */
	char *trailing;
};

//TODO: replace comps to channel / nicks
int irc_strcmp(const char*, const char*);
int irc_strncmp(const char*, const char*, size_t);

char* getarg(char**, const char*);
char* strdup(const char*);
char* word_wrap(int, char**, char*);

int check_pinged(const char*, const char*);
int parse_mesg(struct parsed_mesg*, char*);

void handle_error(int, const char*, ...);

extern int fatal_exit;

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) > (B) ? (B) : (A))

/* Irrecoverable error
 *   this define is precluded in test.h to aggregate fatal errors in testcases */
#ifndef fatal
#define fatal(M, E) \
	do { handle_error(E, "ERROR in %s: %s", __func__, M); } while (0)
#endif

//TODO: refactor
/* Doubly linked list macros */
#define DLL_NEW(L, N) ((L) = (N)->next = (N)->prev = (N))

#define DLL_ADD(L, N) \
	do { \
		if ((L) == NULL) \
			DLL_NEW(L, N); \
		else { \
			((L)->next)->prev = (N); \
			(N)->next = ((L)->next); \
			(N)->prev = (L); \
			(L)->next = (N); \
		} \
	} while (0)

#define DLL_DEL(L, N) \
	do { \
		if (((N)->next) == (N)) \
			(L) = NULL; \
		else { \
			if ((L) == N) \
				(L) = ((N)->next); \
			((N)->next)->prev = ((N)->prev); \
			((N)->prev)->next = ((N)->next); \
		} \
	} while (0)


#endif

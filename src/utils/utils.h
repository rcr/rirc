#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define TO_STR(X) #X
#define STR(X) TO_STR(X)

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) > (B) ? (B) : (A))

#define ELEMS(X) (sizeof((X)) / sizeof((X)[0]))
#define ARR_ELEM(A, E) ((E) >= 0 && (size_t)(E) < ELEMS((A)))

#define UNUSED(X) ((void)(X))

#if (defined DEBUG) && !(defined TESTING)
#define DEBUG_MSG(...) \
	do { fprintf(stderr, "%s:%d:%-12s\t", __FILE__, __LINE__, __func__); \
	     fprintf(stderr,  __VA_ARGS__); \
	     fprintf(stderr, "\n"); \
	} while (0)
#else
#define DEBUG_MSG(...)
#endif

/* Irrecoverable error
 *   precluded in test.h to aggregate fatal errors in testcases */
#ifndef fatal
#define fatal(...) \
	do { fprintf(stderr, "FATAL ERROR: %s:%d:%s: ", __FILE__, __LINE__, __func__); \
	     fprintf(stderr, __VA_ARGS__); \
	     fprintf(stderr, "\n"); \
	     fatal_exit = 1; \
	     exit(EXIT_FAILURE); \
	} while (0)
#endif

/* FIXME: don't seperate trailing from params
 * simplify retrieving/tokenizing arguments
 * from a parsed_mesg struct
 */
/* Parsed IRC message */
struct parsed_mesg
{
	char *from;
	char *host;
	char *command;
	char *params;
	char *trailing;
};

//TODO: replace comps to channel / nicks
int irc_ischanchar(char, int);
int irc_isnickchar(char, int);
int irc_ischan(const char*);
int irc_isnick(const char*);
//TODO: CASEMAPPING
int irc_strcmp(const char*, const char*);
int irc_strncmp(const char*, const char*, size_t);

char* getarg(char**, const char*);
char* word_wrap(int, char**, char*);
char* strdup(const char*);
void* memdup(const void*, size_t);

int check_pinged(const char*, const char*);
int parse_mesg(struct parsed_mesg*, char*);
int skip_sp(char**);

extern int fatal_exit;

#endif

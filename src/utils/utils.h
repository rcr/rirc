#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Parsed IRC message */

/* FIXME: don't seperate trailing from params
 * simplify retrieving/tokenizing arguments
 * from a parsed_mesg struct
 */
struct parsed_mesg
{
	char *from;
	char *host;
	char *command;
	char *params;
	char *trailing;
};

/* Dynamically allocated string with cached length */
struct string
{
	const char *str;
	size_t len;
};

int irc_isnickchar(const char, int);
//TODO: replace comps to channel / nicks
int irc_strcmp(const char*, const char*);
int irc_strncmp(const char*, const char*, size_t);

char* getarg(char**, const char*);
char* word_wrap(int, char**, char*);

char* strdup(const char*);
void* memdup(const void*, size_t);

int check_pinged(const char*, const char*);
int parse_mesg(struct parsed_mesg*, char*);
int skip_sp(char**);

struct string* string(struct string*, const char*);
void string_free(struct string*);

void handle_error(int, const char*, ...);

extern int fatal_exit;

#define TO_STR(X) #X
#define STR(X) TO_STR(X)

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) > (B) ? (B) : (A))

#define ELEMS(X) (sizeof((X)) / sizeof((X)[0]))
#define ARR_ELEM(A, E) ((E) >= 0 && (size_t)(E) < ELEMS((A)))

#define UNUSED(X) ((void)(X))

#if (defined DEBUG) && !(defined TESTING)
#define DEBUG_MSG(...) \
	do { \
		fprintf(stderr, "%s:%d:%-12s\t", __FILE__, __LINE__, __func__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while (0)
#else
#define DEBUG_MSG(...)
#endif

/* Irrecoverable error
 *   this define is precluded in test.h to aggregate fatal errors in testcases */
#ifndef fatal
#define fatal(M, E) \
	do { handle_error(E, "ERROR in %s: %s", __func__, M); exit(EXIT_FAILURE); } while (0)
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

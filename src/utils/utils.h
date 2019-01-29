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

#define MESSAGE(TYPE, ...) \
	fprintf(stderr, "%s %s:%d:%s ", (TYPE), __FILE__, __LINE__, __func__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n");

#if (defined DEBUG) && !(defined TESTING)
#define debug(...) \
	do { MESSAGE("DEBUG", __VA_ARGS__); } while (0)
#else
#define debug(...) \
	do { ; } while (0)
#endif

#ifndef fatal
#define fatal(...) \
	do { MESSAGE("FATAL", __VA_ARGS__); exit(EXIT_FAILURE); } while (0)
#define fatal_noexit(...) \
	do { MESSAGE("FATAL", __VA_ARGS__); } while (0)
#endif

struct irc_message
{
	char *params;
	const char *command;
	const char *from;
	const char *host;
	size_t len_command;
	size_t len_from;
	size_t len_host;
	unsigned n_params;
	unsigned split : 1;
};

int irc_message_param(struct irc_message*, char**);
int irc_message_parse(struct irc_message*, char*, size_t);
int irc_message_split(struct irc_message*, char**);

//TODO: replace comps to channel / nicks
int irc_ischanchar(char, int);
int irc_isnickchar(char, int);
int irc_ischan(const char*);
int irc_isnick(const char*);
//TODO: CASEMAPPING, ascii, rfc, strict
int irc_strcmp(const char*, const char*);
int irc_strncmp(const char*, const char*, size_t);

int str_trim(char**);
int check_pinged(const char*, const char*);
char* getarg(char**, const char*);
char* word_wrap(int, char**, char*);

char* strdup(const char*);
void* memdup(const void*, size_t);

// TODO: replace check_pinged -> irc_pinged, account for casemapping
// TODO: replace word_wrap -> str_wrap... int(char**, char**, size_t)
// TODO: getarg -> simplify, str_token? tokens on ' ' only

#endif

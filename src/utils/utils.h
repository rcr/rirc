#ifndef RIRC_UTILS_UTILS_H
#define RIRC_UTILS_UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define ARR_LEN(A) (sizeof((A)) / sizeof((A)[0]))

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) > (B) ? (B) : (A))

#define SEC_IN_MS(X) ((X) * 1000)
#define SEC_IN_US(X) ((X) * 1000 * 1000)

#define UNUSED(X) ((void)(X))

#define MESSAGE(TYPE, ...) \
	fprintf(stderr, "%s %s:%d:%s ", (TYPE), __FILE__, __LINE__, __func__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	fflush(stderr);

#if !(defined NDEBUG) && !(defined TESTING)
#define debug(...) \
	do { MESSAGE("DEBUG", __VA_ARGS__); } while (0)
#define debug_send(L, M) \
	do { fprintf(stderr, "DEBUG (--> %3zu) %s\n", (L), (M)); fflush(stderr); } while (0)
#define debug_recv(L, M) \
	do { fprintf(stderr, "DEBUG (<-- %3zu) %s\n", (L), (M)); fflush(stderr); } while (0)
#else
#define debug(...) \
	do { ; } while (0)
#define debug_send(L, M) \
	do { ; } while (0)
#define debug_recv(L, M) \
	do { ; } while (0)
#endif

#ifndef fatal
#define fatal(...) \
	do { MESSAGE("FATAL", __VA_ARGS__); exit(EXIT_FAILURE); } while (0)
#define fatal_noexit(...) \
	do { MESSAGE("FATAL", __VA_ARGS__); } while (0)
#endif

enum casemapping
{
	CASEMAPPING_INVALID,
	CASEMAPPING_ASCII,
	CASEMAPPING_RFC1459,
	CASEMAPPING_STRICT_RFC1459
};

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

int irc_ischan(const char*);
int irc_isnick(const char*);
int irc_pinged(enum casemapping, const char*, const char*);
int irc_strcmp(enum casemapping, const char*, const char*);
int irc_strncmp(enum casemapping, const char*, const char*, size_t);
char* irc_strsep(char**);
char* irc_strtrim(char**);
char* irc_strwrap(unsigned, char**, char*);

int irc_message_param(struct irc_message*, char**);
int irc_message_parse(struct irc_message*, char*);
int irc_message_split(struct irc_message*, const char**, const char**);

#endif

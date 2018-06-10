#ifndef RIRC_H
#define RIRC_H

#define BUFFSIZE 512

//TODO: just malloc the current nick
#define NICKSIZE 255

/* Error message length */
#define MAX_ERROR 512

/* Translate defined values to strings at compile time */
#define TO_STR(X) #X
#define STR(X) TO_STR(X)

extern struct config
{
	char *username;
	char *realname;
	char *default_nick;
	unsigned int join_part_quit_threshold;
} config;

#endif

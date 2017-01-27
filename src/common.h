#ifndef COMMON_H
#define COMMON_H

/* FIXME: refactoring */
#include "draw.h"
#include "utils.h"

#define VERSION "0.1"

#define MAX_SERVERS 32

#define NICKSIZE 255

#define BUFFSIZE 512

/* When tab completing a nick at the beginning of the line, append the following char */
#define TAB_COMPLETE_DELIMITER ':'

/* Compile time checks */
#if BUFFSIZE < MAX_INPUT
/* Required so input lines can be safely strcpy'ed into a send buffer */
#error BUFFSIZE must be greater than MAX_INPUT
#endif

/* Error message length */
#define MAX_ERROR 512

/* Message sent for PART and QUIT by default */
#define DEFAULT_QUIT_MESG "rirc v" VERSION

/* Buffer bar activity types */
typedef enum
{
	ACTIVITY_DEFAULT,
	ACTIVITY_ACTIVE,
	ACTIVITY_PINGED,
	ACTIVITY_T_SIZE
} activity_t;

#include "channel.h"
#include "server.h"
#include "mesg.h"
#include "rirc.h"

#endif

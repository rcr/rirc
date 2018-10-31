#include <stdlib.h>
#include <string.h>

#include "src/components/channel.h"
#include "src/utils/utils.h"

static inline int channel_cmp(struct channel*, const char*);

static inline int
channel_cmp(struct channel *c, const char *name)
{
	/* TODO: CASEMAPPING, as ftpr held by the server */
	return irc_strcmp(c->name, name);
}

struct channel*
channel(const char *name, enum channel_t type)
{
	struct channel *c;

	size_t len = strlen(name);

	if ((c = calloc(1, sizeof(*c) + len + 1)) == NULL)
		fatal("calloc", errno);

	c->chanmodes_str.type = MODE_STR_CHANMODE;
	c->name_len = len;
	c->type = type;

	memcpy(c->name, name, len + 1);

	buffer(&c->buffer);
	input_init(&c->input);

	return c;
}

void
channel_free(struct channel *c)
{
	input_free(&c->input);
	user_list_free(&(c->users));
	free(c);
}

void
channel_list_free(struct channel_list *cl)
{
	/* TODO */;
}

struct channel*
channel_list_add(struct channel_list *cl, struct channel *c)
{
	struct channel *tmp;

	if ((tmp = channel_list_get(cl, c->name)) != NULL)
		return tmp;

	if (cl->head == NULL) {
		cl->head = c->next = c;
		cl->tail = c->prev = c;
	} else {
		c->next = cl->tail->next;
		c->prev = cl->tail;
		cl->head->prev = c;
		cl->tail->next = c;
		cl->tail = c;
	}

	return NULL;
}

// TODO: segault when deleting the tail and try to `prev` the head

struct channel*
channel_list_del(struct channel_list *cl, struct channel *c)
{
	struct channel *tmp_h,
	               *tmp_t;

	if (cl->head == NULL) {
		return NULL;
	} else if (cl->head == c && cl->tail == c) {
		/* Removing tail */
		cl->head = NULL;
		cl->tail = NULL;
	} else if ((tmp_h = cl->head) == c) {
		/* Removing head */
		cl->head = cl->tail->next = cl->head->next;
		cl->head->prev = cl->tail;
	} else if ((tmp_t = cl->tail) == c) {
		/* Removing tail */
		cl->tail = cl->head->prev = cl->tail->prev;
		cl->tail->next = cl->head;
	} else {
		/* Removing some channel (head, tail) */
		while ((tmp_h = tmp_h->next) != c) {
			if (tmp_h == tmp_t)
				return NULL;
		}
		c->next->prev = c->prev;
		c->prev->next = c->next;
	}

	c->next = NULL;
	c->prev = NULL;

	return c;
}

struct channel*
channel_list_get(struct channel_list *cl, const char *name)
{
	struct channel *tmp;

	if ((tmp = cl->head) == NULL)
		return NULL;

	if (!channel_cmp(cl->head, name))
		return cl->head;

	while ((tmp = tmp->next) != cl->head) {

		if (!channel_cmp(tmp, name))
			return tmp;
	}

	return NULL;
}

void
channel_part(struct channel *c)
{
	channel_reset(c);
	c->parted = 1;
}

void
channel_reset(struct channel *c)
{
	mode_reset(&(c->chanmodes), &(c->chanmodes_str));
	user_list_free(&(c->users));
}

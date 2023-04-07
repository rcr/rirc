#include "src/components/channel.h"

#include "src/utils/utils.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct channel*
channel(const char *name, enum channel_type type)
{
	struct channel *c;

	size_t len = strlen(name);

	if ((c = calloc(1, sizeof(*c) + len + 1)) == NULL)
		fatal("calloc: %s", strerror(errno));

	c->name_len = len;
	c->name = memcpy(c->_, name, len + 1);
	c->type = type;

	buffer(&c->buffer);
	input_init(&c->input);

	return c;
}

void
channel_free(struct channel *c)
{
	input_free(&c->input);
	user_list_free(&(c->users));
	free((void *)c->key);
	free(c);
}

void
channel_list_free(struct channel_list *cl)
{
	struct channel *c1, *c2;

	if ((c1 = cl->head) == NULL)
		return;

	do {
		c2 = c1;
		c1 = c2->next;
		channel_free(c2);
	} while (c1 != cl->head);
}

void
channel_key_add(struct channel *c, const char *key)
{
	free((void *)c->key);
	c->key = irc_strdup(key);
}

void
channel_key_del(struct channel *c)
{
	free((void *)c->key);
	c->key = NULL;
}

void
channel_list_add(struct channel_list *cl, struct channel *c)
{
	cl->count++;

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
}

void
channel_list_del(struct channel_list *cl, struct channel *c)
{
	cl->count--;

	if (cl->head == c && cl->tail == c) {
		cl->head = NULL;
		cl->tail = NULL;
	} else if (cl->head == c) {
		cl->head = cl->tail->next = cl->head->next;
		cl->head->prev = cl->tail;
	} else if (cl->tail == c) {
		cl->tail = cl->head->prev = cl->tail->prev;
		cl->tail->next = cl->head;
	} else {
		c->next->prev = c->prev;
		c->prev->next = c->next;
	}

	c->next = NULL;
	c->prev = NULL;
}

struct channel*
channel_list_get(struct channel_list *cl, const char *name, enum casemapping cm)
{
	struct channel *tmp;

	if ((tmp = cl->head) == NULL)
		return NULL;

	if (!irc_strcmp(cm, cl->head->name, name))
		return cl->head;

	while ((tmp = tmp->next) != cl->head) {
		if (!irc_strcmp(cm, tmp->name, name))
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
	memset(&(c->chanmodes), 0, sizeof(c->chanmodes));
	memset(&(c->chanmodes_str), 0, sizeof(c->chanmodes_str));
	user_list_free(&(c->users));
	c->joined = 0;
}

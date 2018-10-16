#include <string.h>

#include "input2.h"

static int input2_del_back(struct input2*);
static int input2_del_forw(struct input2*);
static int input2_move_back(struct input2*);
static int input2_move_forw(struct input2*);
static unsigned input2_size(struct input2*);

int
input2_empty(struct input2 *inp)
{
	return (input2_size(inp) == 0);
}

int
input2_full(struct input2 *inp)
{
	return (input2_size(inp) == INPUT_LEN_MAX);
}

void
input2(struct input2 *inp)
{
	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;
}

int
input2_del(struct input2 *inp, int forw)
{
	if (forw)
		return input2_del_forw(inp);
	else
		return input2_del_back(inp);
}

int
input2_move(struct input2 *inp, int forw)
{
	if (forw)
		return input2_move_forw(inp);
	else
		return input2_move_back(inp);
}

int
input2_ins(struct input2 *inp, const char *c, size_t count)
{
	size_t i = count;

	while (!input2_full(inp) && i--) {
		inp->text[inp->head++] = *c++;
	}

	return (i != count);
}

char*
input2_write(struct input2 *inp, char *buf, size_t max)
{
	/* Write the input to `buf` as a null terminated string */

	uint16_t i = 0, j = 0;

	while (max > 1 && i < inp->head) {
		buf[j++] = inp->text[i++];
		max--;
	}

	i = inp->tail;

	while (max > 1 && i < INPUT_LEN_MAX) {
		buf[j++] = inp->text[i++];
		max--;
	}

	buf[j] = 0;

	return buf;
}

static int
input2_del_back(struct input2 *inp)
{
	if (inp->head == 0)
		return 0;

	inp->head--;

	return 1;
}

static int
input2_del_forw(struct input2 *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->tail++;

	return 1;
}

static int
input2_move_back(struct input2 *inp)
{
	if (inp->head == 0)
		return 0;

	inp->text[--inp->tail] = inp->text[--inp->head];

	return 1;
}

static int
input2_move_forw(struct input2 *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->text[inp->head++] = inp->text[inp->tail++];

	return 1;
}

static unsigned
input2_size(struct input2 *inp)
{
	return (inp->head + (inp->tail - INPUT_LEN_MAX));
}

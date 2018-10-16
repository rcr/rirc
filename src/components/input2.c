#include <string.h>

#include "src/components/input2.h"
#include "src/utils/utils.h"

static int input2_del_back(struct input2*);
static int input2_del_forw(struct input2*);
static int input2_hist_back(struct input2*);
static int input2_hist_forw(struct input2*);
static int input2_isfull(struct input2*);
static int input2_iszero(struct input2*);
static int input2_move_back(struct input2*);
static int input2_move_forw(struct input2*);
static unsigned input2_size(struct input2*);

void
input2(struct input2 *inp)
{
	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;
}

void
input2_free(struct input2 *inp)
{
	(void)inp;
}

int
input2_clear(struct input2 *inp)
{
	/* TODO: should reset from history */

	if (input2_iszero(inp))
		return 0;

	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;

	return 1;
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
input2_ins(struct input2 *inp, const char *c, size_t count)
{
	size_t i = count;

	while (!input2_isfull(inp) && i--) {
		inp->text[inp->head++] = *c++;
	}

	return (i != count);
}

int
input2_hist(struct input2 *inp, int forw)
{
	if (forw)
		return input2_hist_forw(inp);
	else
		return input2_hist_back(inp);
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
input2_push(struct input2 *inp)
{
	/* Push the current input line to history */

	unsigned size;

	if ((size = input2_size(inp)) == 0)
		return 0;

	// TODO: must:
	//  add the current line to history
	//  del the hist tail if full
	//  add a new line context to the head

	// save the whole input line struct? or just linked list of const char*,
	// copied into working input area on scroll?

	return 1;
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
input2_isfull(struct input2 *inp)
{
	return (input2_size(inp) == INPUT_LEN_MAX);
}

static int
input2_iszero(struct input2 *inp)
{
	return (input2_size(inp) == 0);
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
input2_hist_back(struct input2 *inp)
{
	/* TODO */
	(void)inp;
	return 0;
}

static int
input2_hist_forw(struct input2 *inp)
{
	/* TODO */
	(void)inp;
	return 0;
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

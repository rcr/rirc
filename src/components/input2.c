#include <string.h>

#include "src/components/input2.h"
#include "src/utils/utils.h"

#if (INPUT_HIST_MAX & (INPUT_HIST_MAX - 1)) != 0
/* Required for proper masking when indexing */
#error INPUT_HIST_MAX must be a power of 2
#endif

#define MASK(X) ((X) & (INPUT_HIST_MAX - 1))

static int input2_cursor_back(struct input2*);
static int input2_cursor_forw(struct input2*);
static int input2_delete_back(struct input2*);
static int input2_delete_forw(struct input2*);
static int input2_text_isfull(struct input2*);
static int input2_text_iszero(struct input2*);
static size_t input2_hist_size(struct input2*);
static size_t input2_text_size(struct input2*);

void
input2(struct input2 *inp)
{
	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;
	inp->hist.current = 0;
	inp->hist.head = 0;
	inp->hist.tail = 0;
}

void
input2_free(struct input2 *inp)
{
	while (inp->hist.tail != inp->hist.head)
		free(inp->hist.buf[MASK(inp->hist.tail++)]);
}

int
input2_clear(struct input2 *inp)
{
	/* TODO: should reset from history */

	if (input2_text_iszero(inp))
		return 0;

	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;

	return 1;
}

int
input2_cursor(struct input2 *inp, int forw)
{
	if (forw)
		return input2_cursor_forw(inp);
	else
		return input2_cursor_back(inp);
}

int
input2_delete(struct input2 *inp, int forw)
{
	if (forw)
		return input2_delete_forw(inp);
	else
		return input2_delete_back(inp);
}

int
input2_insert(struct input2 *inp, const char *c, size_t count)
{
	size_t i = count;

	while (!input2_text_isfull(inp) && i--) {
		inp->text[inp->head++] = *c++;
	}

	return (i != count);
}

int
input2_complete(struct input2 *inp, f_completion_cb cb)
{
	/* TODO: should pass to the callback:
	 *  - the complete word under the cursor
	 *  - flag if the word is the line leader
	 *
	 * callback returns a string to replace the word under the cursor
	 * and returns 0 if no change was made
	 */

	(void)inp;
	(void)cb;
	return 0;
}

int
input2_hist_back(struct input2 *inp)
{
	/* TODO */
	(void)inp;
	return 0;
}

int
input2_hist_forw(struct input2 *inp)
{
	/* TODO */
	(void)inp;
	return 0;
}

int
input2_hist_push(struct input2 *inp)
{
	char *str;
	size_t len;

	if ((len = input2_text_size(inp)) == 0)
		return 0;

	if ((str = malloc(len + 1)) == NULL)
		fatal("malloc", errno);

	input2_write(inp, str, len + 1);

	if (inp->hist.current) {
		; // TODO: move hist to head
	} else if (input2_hist_size(inp) == INPUT_HIST_MAX) {
		free(inp->hist.buf[MASK(inp->hist.tail++)]);
	}

	inp->hist.buf[MASK(inp->hist.head++)] = str;
	inp->hist.current = 0;

	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;

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
input2_cursor_back(struct input2 *inp)
{
	if (inp->head == 0)
		return 0;

	inp->text[--inp->tail] = inp->text[--inp->head];

	return 1;
}

static int
input2_cursor_forw(struct input2 *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->text[inp->head++] = inp->text[inp->tail++];

	return 1;
}

static int
input2_delete_back(struct input2 *inp)
{
	if (inp->head == 0)
		return 0;

	inp->head--;

	return 1;
}

static int
input2_delete_forw(struct input2 *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->tail++;

	return 1;
}

static int
input2_text_isfull(struct input2 *inp)
{
	return (input2_text_size(inp) == INPUT_LEN_MAX);
}

static int
input2_text_iszero(struct input2 *inp)
{
	return (input2_text_size(inp) == 0);
}

static size_t
input2_text_size(struct input2 *inp)
{
	return (inp->head + (inp->tail - INPUT_LEN_MAX));
}

static size_t
input2_hist_size(struct input2 *inp)
{
	return (inp->hist.head - inp->hist.tail);
}

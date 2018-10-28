#include <string.h>

#include "src/components/input.h"
#include "src/utils/utils.h"

#define INPUT_MASK(X) ((X) & (INPUT_HIST_MAX - 1))

#if INPUT_MASK(INPUT_HIST_MAX)
/* Required for proper masking when indexing */
#error INPUT_HIST_MAX must be a power of 2
#endif

static char *input_text_copy(struct input*);
static int input_text_isfull(struct input*);
static int input_text_iszero(struct input*);
static uint16_t input_hist_size(struct input*);
static uint16_t input_text_size(struct input*);

void
input(struct input *inp)
{
	memset(inp, 0, sizeof(*inp));

	inp->tail = INPUT_LEN_MAX;
}

void
input_free(struct input *inp)
{
	free(inp->hist.save);

	while (inp->hist.tail != inp->hist.head)
		free(inp->hist.ptrs[INPUT_MASK(inp->hist.tail++)]);
}

int
input_cursor_back(struct input *inp)
{
	if (inp->head == 0)
		return 0;

	inp->text[--inp->tail] = inp->text[--inp->head];

	return 1;
}

int
input_cursor_forw(struct input *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->text[inp->head++] = inp->text[inp->tail++];

	return 1;
}

int
input_delete_back(struct input *inp)
{
	if (inp->head == 0)
		return 0;

	inp->head--;

	return 1;
}

int
input_delete_forw(struct input *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->tail++;

	return 1;
}

int
input_insert(struct input *inp, const char *c, size_t count)
{
	/* TODO: may want to discard control characters */

	size_t i = count;

	while (!input_text_isfull(inp) && i--) {
		inp->text[inp->head++] = *c++;
	}

	return (i != count);
}

int
input_reset(struct input *inp)
{
	if (input_text_iszero(inp))
		return 0;

	free(inp->hist.save);

	inp->hist.current = inp->hist.head;
	inp->hist.save = NULL;

	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;

	return 1;
}

int
input_complete(struct input *inp, f_completion_cb cb)
{
	/* Completion is valid when the cursor is:
	 *  - above any character in a word
	 *  - above a space immediately following a word
	 *  - at the end of a line following a word */

	uint16_t head, tail, ret;

	if (input_text_iszero(inp))
		return 0;

	head = inp->head;
	tail = inp->tail;

	while (head && inp->text[head - 1] != ' ')
		head--;

	if (inp->text[head] == ' ')
		return 0;

	while (tail < INPUT_LEN_MAX && inp->text[tail] != ' ')
		tail++;

	ret = (*cb)(
		(inp->text + head),
		(inp->head - head - inp->tail + tail),
		(INPUT_LEN_MAX - input_text_size(inp)),
		(head == 0));

	if (ret) {
		inp->head = head + ret;
		inp->tail = tail;
	}

	return (ret != 0);
}

int
input_hist_back(struct input *inp)
{
	size_t len;

	if (input_hist_size(inp) == 0 || inp->hist.current == inp->hist.tail)
		return 0;

	if (inp->hist.current == inp->hist.head) {
		inp->hist.save = input_text_copy(inp);
	} else {
		free(inp->hist.ptrs[INPUT_MASK(inp->hist.current)]);
		inp->hist.ptrs[INPUT_MASK(inp->hist.current)] = input_text_copy(inp);
	}

	inp->hist.current--;

	len = strlen(inp->hist.ptrs[INPUT_MASK(inp->hist.current)]);
	memcpy(inp->text, inp->hist.ptrs[INPUT_MASK(inp->hist.current)], len);

	inp->head = len;
	inp->tail = INPUT_LEN_MAX;

	return 1;
}

int
input_hist_forw(struct input *inp)
{
	char *str;
	size_t len;

	if (input_hist_size(inp) == 0 || inp->hist.current == inp->hist.head)
		return 0;

	free(inp->hist.ptrs[INPUT_MASK(inp->hist.current)]);
	inp->hist.ptrs[INPUT_MASK(inp->hist.current)] = input_text_copy(inp);

	inp->hist.current++;

	if (inp->hist.current == inp->hist.head)
		str = inp->hist.save;
	else
		str = inp->hist.ptrs[INPUT_MASK(inp->hist.current)];

	len = strlen(str);
	memcpy(inp->text, str, len);

	if (inp->hist.current == inp->hist.head)
		free(inp->hist.save);

	inp->head = len;
	inp->tail = INPUT_LEN_MAX;

	return 1;
}

int
input_hist_push(struct input *inp)
{
	char *save;

	if ((save = input_text_copy(inp)) == NULL)
		return 0;

	if (inp->hist.current < inp->hist.head) {

		uint16_t i;

		free(inp->hist.ptrs[INPUT_MASK(inp->hist.current)]);

		for (i = inp->hist.current; i < inp->hist.head - 1; i++)
			inp->hist.ptrs[INPUT_MASK(i)] = inp->hist.ptrs[INPUT_MASK(i + 1)];

		inp->hist.current = i;
		inp->hist.ptrs[INPUT_MASK(inp->hist.current)] = save;

	} else if (input_hist_size(inp) == INPUT_HIST_MAX) {

		free(inp->hist.ptrs[INPUT_MASK(inp->hist.tail++)]);
		inp->hist.ptrs[INPUT_MASK(inp->hist.head++)] = save;

	} else {

		inp->hist.ptrs[INPUT_MASK(inp->hist.head++)] = save;
	}

	return input_reset(inp);
}

char*
input_write(struct input *inp, char *buf, size_t max)
{
	/* Write the input to `buf` as a null terminated string */

	/* TODO input framing */
#if 0
static inline void
reframe_line(struct input *in, unsigned int cols)
{
	/* Reframe a line's draw window */

	in->head = in->line->end;
	in->tail = in->line->text + RIRC_MAX_INPUT;
	in->window = in->head - (2 * cols / 3);

	if (in->window < in->line->text)
		in->window = in->line->text;
}
#endif

	uint16_t i = 0,
	         j = 0;

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

static char*
input_text_copy(struct input *inp)
{
	char *str;
	size_t len;

	if ((len = input_text_size(inp)) == 0)
		return 0;

	if ((str = malloc(len + 1)) == NULL)
		fatal("malloc", errno);

	return input_write(inp, str, len + 1);
}

static int
input_text_isfull(struct input *inp)
{
	return (input_text_size(inp) == INPUT_LEN_MAX);
}

static int
input_text_iszero(struct input *inp)
{
	return (input_text_size(inp) == 0);
}

static uint16_t
input_text_size(struct input *inp)
{
	return (inp->head + (INPUT_LEN_MAX - inp->tail));
}

static uint16_t
input_hist_size(struct input *inp)
{
	return (inp->hist.head - inp->hist.tail);
}

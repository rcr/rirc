#include "src/components/input.h"

#include "src/utils/utils.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#define INPUT_MASK(X) ((X) & (INPUT_HIST_MAX - 1))

#if INPUT_MASK(INPUT_HIST_MAX)
/* Required for proper masking when indexing */
#error INPUT_HIST_MAX must be a power of 2
#endif

#define INPUT_HIST_LINE(I, X) ((I)->hist.ptrs[INPUT_MASK((X))])

static char *input_text_copy(struct input*);
static int input_text_isfull(struct input*);
static int input_text_iszero(struct input*);
static uint16_t input_hist_size(struct input*);
static uint16_t input_text_size(struct input*);

void
input_init(struct input *inp)
{
	memset(inp, 0, sizeof(*inp));

	inp->tail = INPUT_LEN_MAX;
}

void
input_free(struct input *inp)
{
	while (inp->hist.tail != inp->hist.head)
		free(INPUT_HIST_LINE(inp, inp->hist.tail++));
}

int
input_cursor_back(struct input *inp)
{
	if (inp->head == 0)
		return 0;

	inp->buf[--inp->tail] = inp->buf[--inp->head];

	return 1;
}

int
input_cursor_forw(struct input *inp)
{
	if (inp->tail == INPUT_LEN_MAX)
		return 0;

	inp->buf[inp->head++] = inp->buf[inp->tail++];

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
	if (input_text_isfull(inp))
		return 0;

	while (!input_text_isfull(inp) && count--) {

		if (iscntrl(*c))
			inp->buf[inp->head++] = ' ';

		if (isprint(*c))
			inp->buf[inp->head++] = *c;

		c++;
	}

	return 1;
}

int
input_reset(struct input *inp)
{
	if (input_text_iszero(inp))
		return 0;

	inp->hist.current = inp->hist.head;
	inp->head = 0;
	inp->tail = INPUT_LEN_MAX;
	inp->window = 0;

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

	while (head && inp->buf[head - 1] != ' ')
		head--;

	if (inp->buf[head] == ' ')
		return 0;

	while (tail < INPUT_LEN_MAX && inp->buf[tail] != ' ')
		tail++;

	ret = (*cb)(
		(inp->buf + head),
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

	inp->hist.current--;

	len = strlen(INPUT_HIST_LINE(inp, inp->hist.current));
	memcpy(inp->buf, INPUT_HIST_LINE(inp, inp->hist.current), len);

	inp->head = len;
	inp->tail = INPUT_LEN_MAX;

	return 1;
}

int
input_hist_forw(struct input *inp)
{
	size_t len;

	if (input_hist_size(inp) == 0 || inp->hist.current == inp->hist.head)
		return 0;

	inp->hist.current++;

	if (inp->hist.current == inp->hist.head) {
		len = 0;
	} else {
		len = strlen(INPUT_HIST_LINE(inp, inp->hist.current));
		memcpy(inp->buf, INPUT_HIST_LINE(inp, inp->hist.current), len);
	}

	inp->head = len;
	inp->tail = INPUT_LEN_MAX;

	return 1;
}

int
input_hist_push(struct input *inp)
{
	char *hist;

	if ((hist = input_text_copy(inp)) == NULL)
		return 0;

	if (input_hist_size(inp) == INPUT_HIST_MAX)
		free(INPUT_HIST_LINE(inp, inp->hist.tail++));

	INPUT_HIST_LINE(inp, inp->hist.head++) = hist;

	return input_reset(inp);
}

uint16_t
input_frame(struct input *inp, char *buf, uint16_t max)
{
	/*  Keep the input head in view, reframing if the cursor would be
	 *  drawn outside [A, B] as a function of the given max width
	 *
	 * 0       W             W + M
	 * |-------|---------------|------
	 * |       |A             B|
	 *         |<--         -->| : max
	 *
	 * The cursor should track the input head, where the next
	 * character would be entered
	 *
	 * In the <= A case: deletions occurred since previous reframe;
	 * the head is less than or equal to the window
	 *
	 * In the >= B case: insertions occurred since previous reframe;
	 * the distance from window to head is greater than the distance
	 * from [A, B]
	 *
	 * Set the window 2/3 of the text area width backwards from the head
	 * and returns the cursor position relative to the window */

	if (inp->window >= inp->head || (inp->window + (max - 1)) <= inp->head)
		inp->window = (((max - 1) * 2 / 3) >= inp->head) ? 0 : inp->head - ((max - 1) * 2 / 3);

	input_write(inp, buf, max, inp->window);

	return (inp->head - inp->window);
}

uint16_t
input_write(struct input *inp, char *buf, uint16_t max, uint16_t pos)
{
	uint16_t buf_len = 0;

	while (max > 1 && pos < inp->head) {
		buf[buf_len++] = inp->buf[pos++];
		max--;
	}

	pos = inp->tail;

	while (max > 1 && pos < INPUT_LEN_MAX) {
		buf[buf_len++] = inp->buf[pos++];
		max--;
	}

	buf[buf_len] = 0;

	return buf_len;
}

static char*
input_text_copy(struct input *inp)
{
	char *str;
	size_t len;

	if ((len = input_text_size(inp)) == 0)
		return NULL;

	if ((str = malloc(len + 1)) == NULL)
		fatal("malloc: %s", strerror(errno));

	input_write(inp, str, len + 1, 0);

	return str;
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

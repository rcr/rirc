#ifndef INPUT_H
#define INPUT_H

/* Buffer input
 *
 * Supports line editing, input history, word completion
 *
 * The working edit area is implemented as a fixed width
 * gap buffer for O(1) insertions, deltions and O(n)
 * cursor movements
 *
 * Input history is kept as a ring buffer of strings,
 * copied into the working area when scrolling
 */

#include <stdint.h>

/* 410 max characters for input should be sufficient given
 * rfc2812 maximum length of 50 characters for channel names,
 * plus 50 characters for additional message formatting */
#ifndef INPUT_LEN_MAX
#define INPUT_LEN_MAX 410
#endif

/* Number of history lines to keep for input. For proper
 * ring buffer masking this must be a power of 2 */
#ifndef INPUT_HIST_MAX
#define INPUT_HIST_MAX 16
#endif

/* Input completion callback type, returning length
 * of replacement word, or 0 if no match, with args: */
typedef uint16_t (*f_completion_cb)(
	char*,    /* word to replace */
	uint16_t, /* length of word */
	uint16_t, /* max length of replacement word */
	int);     /* flag set if word is start of input */

struct input2
{
	char text[INPUT_LEN_MAX];
	struct {
		char *ptrs[INPUT_HIST_MAX];
		char *save;
		uint16_t current;
		uint16_t head;
		uint16_t tail;
	} hist;
	uint16_t head;
	uint16_t tail;
};

void input2(struct input2*);
void input2_free(struct input2*);

/* Input manipulation */
int input2_cursor_back(struct input2*);
int input2_cursor_forw(struct input2*);
int input2_delete_back(struct input2*);
int input2_delete_forw(struct input2*);
int input2_insert(struct input2*, const char*, size_t);
int input2_reset(struct input2*);

/* Input completion */
int input2_complete(struct input2*, f_completion_cb);

/* Input history */
int input2_hist_back(struct input2*);
int input2_hist_forw(struct input2*);
int input2_hist_push(struct input2*);

/* Write input to string */
char* input2_write(struct input2*, char*, size_t);

#endif

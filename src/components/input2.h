#ifndef INPUT_H
#define INPUT_H

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

/* Input completion callback type */
typedef const char* (*f_completion_cb)(const char*, int);

struct input2
{
	char text[INPUT_LEN_MAX];
	struct {
		char *buf[INPUT_HIST_MAX];
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
int input2_clear(struct input2*);
int input2_cursor(struct input2*, int);
int input2_delete(struct input2*, int);
int input2_insert(struct input2*, const char*, size_t);

/* Input completion */
int input2_complete(struct input2*, f_completion_cb);

/* Input history */
int input2_hist_back(struct input2*);
int input2_hist_forw(struct input2*);
int input2_hist_push(struct input2*);

/* Write input to string */
char* input2_write(struct input2*, char*, size_t);

#endif

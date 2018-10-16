#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* 410 max characters for input should be sufficient given
 * rfc2812 maximum length of 50 characters for channel names,
 * plus 50 characters for additional message formatting */
#ifndef INPUT_LEN_MAX
#define INPUT_LEN_MAX 410
#endif

#ifndef INPUT_HIST_MAX
#define INPUT_HIST_MAX 16
#endif

struct input2_hist
{
	const char *text;
	struct input2_hist *prev;
	struct input2_hist *next;
};

struct input2
{
	char text[INPUT_LEN_MAX];
	struct {
		struct input2_hist *head;
		struct input2_hist *tail;
	} hist;
	uint16_t head;
	uint16_t tail;
};

/* Initialize input */
void input2(struct input2*);
void input2_free(struct input2*);

/* Input manipulation */
int input2_clear(struct input2*);
int input2_del(struct input2*, int);
int input2_ins(struct input2*, const char*, size_t);
int input2_hist(struct input2*, int);
int input2_move(struct input2*, int);
int input2_push(struct input2*);

/* Write input to string */
char* input2_write(struct input2*, char*, size_t);

#endif

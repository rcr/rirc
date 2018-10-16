#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* 410 max characters for input should be sufficient given
 * rfc2812 maximum length of 50 characters for channel names,
 * plus 50 characters for additional message formatting */
#ifndef INPUT_LEN_MAX
#define INPUT_LEN_MAX 410
#endif

struct input2
{
	char text[INPUT_LEN_MAX];
	uint16_t head;
	uint16_t tail;
};

void input2(struct input2*);

int input2_empty(struct input2*);
int input2_full(struct input2*);

int input2_del(struct input2*, int);
int input2_ins(struct input2*, const char*, size_t);

int input2_move(struct input2*, int);

/* Write the current input to string */
char* input2_write(struct input2*, char*, size_t);

#endif

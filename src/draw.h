#ifndef RIRC_DRAW_H
#define RIRC_DRAW_H

enum draw_bit
{
	DRAW_INVALID,
	DRAW_FLUSH,       /* immediately draw all set bits */
	DRAW_BELL,        /* set bit to print terminal bell */
	DRAW_BUFFER,      /* set bit to draw buffer */
	DRAW_BUFFER_BACK, /* set bit to draw buffer scrollback back */
	DRAW_BUFFER_FORW, /* set bit to draw buffer scrollback forward */
	DRAW_INPUT,       /* set bit to draw input */
	DRAW_NAV,         /* set bit to draw nav */
	DRAW_STATUS,      /* set bit to draw status */
	DRAW_ALL,         /* set all draw bits aside from bell */
	DRAW_CLEAR,       /* clear the terminal */
};

void draw_init(void);
void draw_term(void);

void draw(enum draw_bit);

#endif

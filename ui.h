#include <sys/ioctl.h>
struct winsize w;

void resize(void);
void draw_full(void);
void draw_chat(void);
void draw_chans(void);
void print_line(char*, int, int);

#define MAXINPUT 200

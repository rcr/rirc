#ifndef MESG_H
#define MESG_H

void init_mesg(void);
void free_mesg(void);
void recv_mesg(char*, int, server*);
void send_mesg(char*, channel*);
void send_paste(char*);
extern avl_node* commands;

#endif

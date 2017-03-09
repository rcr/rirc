#ifndef MESG_H
#define MESG_H

/* Message sent for PART and QUIT by default */
#define DEFAULT_QUIT_MESG "rirc v" VERSION

void init_mesg(void);
void free_mesg(void);
void recv_mesg(char*, int, server*);
void send_mesg(char*, channel*);
void send_paste(char*);
extern struct avl_node* commands;

#endif

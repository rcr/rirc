#ifndef IRC_CTCP_H
#define IRC_CTCP_H

/* Summary of CTCP implementation:
 *
 *  NOTICE <targ> :\x01ACTION <text>\x01
 *  NOTICE <targ> :\x01CLIENTINFO 1*<arg>\x01
 *  NOTICE <targ> :\x01ERRMSG <text>\x01
 *  NOTICE <targ> :\x01PING 1*<arg>\x01
 *  NOTICE <targ> :\x01SOURCE <text>\x01
 *  NOTICE <targ> :\x01TIME <text>\x01
 *  NOTICE <targ> :\x01VERSION <text>\x01
 */

int recv_ctcp_request(struct server*, struct parsed_mesg*);
int recv_ctcp_response(struct server*, struct parsed_mesg*);

#endif

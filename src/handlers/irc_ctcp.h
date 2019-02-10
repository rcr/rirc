#ifndef IRC_CTCP_H
#define IRC_CTCP_H

/* Summary of CTCP implementation:
 *
 *  NOTICE <targ> :\x01ACTION <text>\x01
 *  NOTICE <targ> :\x01CLIENTINFO 1*<arg>\x01
 *  NOTICE <targ> :\x01FINGER <text>\x01
 *  NOTICE <targ> :\x01PING 1*<arg>\x01
 *  NOTICE <targ> :\x01SOURCE <text>\x01
 *  NOTICE <targ> :\x01TIME <text>\x01
 *  NOTICE <targ> :\x01USERINFO <text>\x01
 *  NOTICE <targ> :\x01VERSION <text>\x01
 */

#define IS_CTCP(M) ((M)[0] == 0x01)

int ctcp_request(struct server*, const char*, const char*, char*);
int ctcp_response(struct server*, const char*, const char*, char*);

#endif

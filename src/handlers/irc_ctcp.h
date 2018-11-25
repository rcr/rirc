#ifndef IRC_CTCP_H
#define IRC_CTCP_H

/* Summary of CTCP implementation:
 *
 * ACTION <text>
 *  :\x01ACTION <text>\x01
 *
 * CLIENTINFO
 *  :\x01CLIENTINFO 1*<arg>\x01
 *
 * ERRMSG
 *  :\x01ERRMSG <text>\x01
 *
 * FINGER
 *  :\x01FINGER <text>\x01
 *
 * PING 1*<arg>
 *  :\x01PING 1*<arg>\x01
 *
 * SOURCE
 *  :\x01SOURCE <text>\x01
 *
 * TIME
 *  :\x01TIME <text>\x01
 *
 * USERINFO
 *  :\x01USERINFO <text>\x01
 *
 * VERSION
 *  :\x01VERSION <text>\x01
 */

int recv_ctcp_request(struct server*, struct parsed_mesg*);
int recv_ctcp_response(struct server*, struct parsed_mesg*);
int send_ctcp(struct server*, struct channel*, char*);

#endif

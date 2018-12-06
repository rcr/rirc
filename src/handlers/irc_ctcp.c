#include "src/components/channel.h"
#include "src/components/server.h"
#include "src/handlers/irc_ctcp.gperf.out"
#include "src/utils/utils.h"

int
recv_ctcp_request(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	(void)recv_ctcp_request_action;
	(void)recv_ctcp_request_clientinfo;
	(void)recv_ctcp_request_errmsg;
	(void)recv_ctcp_request_finger;
	(void)recv_ctcp_request_ping;
	(void)recv_ctcp_request_source;
	(void)recv_ctcp_request_time;
	(void)recv_ctcp_request_userinfo;
	(void)recv_ctcp_request_version;
	return 0;
}

int
recv_ctcp_response(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	(void)recv_ctcp_response_action;
	(void)recv_ctcp_response_clientinfo;
	(void)recv_ctcp_response_errmsg;
	(void)recv_ctcp_response_finger;
	(void)recv_ctcp_response_ping;
	(void)recv_ctcp_response_source;
	(void)recv_ctcp_response_time;
	(void)recv_ctcp_response_userinfo;
	(void)recv_ctcp_response_version;
	return 0;
}

static int recv_ctcp_request_action(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_clientinfo(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_errmsg(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_finger(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_ping(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_source(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_time(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_userinfo(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_request_version(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }

static int recv_ctcp_response_action(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_clientinfo(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_errmsg(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_finger(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_ping(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_source(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_time(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_userinfo(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ctcp_response_version(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }

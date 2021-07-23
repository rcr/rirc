#include "src/components/ircv3.h"

#include <stdlib.h>
#include <string.h>

struct ircv3_cap*
ircv3_cap_get(struct ircv3_caps *caps, const char *cap_str)
{
	#define X(CAP, VAR, ATTRS) \
	if (!strcmp(cap_str, CAP)) \
		return &(caps->VAR);
	IRCV3_CAPS
	#undef X

	return NULL;
}

void
ircv3_caps(struct ircv3_caps *caps)
{
	#define X(CAP, VAR, ATTRS) \
	caps->VAR.val = NULL; \
	caps->VAR.req = 0; \
	caps->VAR.set = 0; \
	caps->VAR.supported = 0; \
	caps->VAR.supports_del = !(ATTRS & IRCV3_CAP_NO_DEL); \
	caps->VAR.supports_req = !(ATTRS & IRCV3_CAP_NO_REQ); \
	caps->VAR.req_auto = (ATTRS & IRCV3_CAP_AUTO);
	IRCV3_CAPS
	#undef X
}

void
ircv3_caps_reset(struct ircv3_caps *caps)
{
	#define X(CAP, VAR, ATTRS) \
	free((void *)caps->VAR.val); \
	caps->VAR.val = NULL; \
	caps->VAR.req = 0; \
	caps->VAR.set = 0; \
	caps->VAR.supported = 0;
	IRCV3_CAPS
	#undef X
}

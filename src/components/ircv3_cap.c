#include "src/components/ircv3_cap.h"

#include <string.h>

enum ircv3_cap_err
{
	IRCV3_CAP_ERR_NONE,
	IRCV3_CAP_ERR_NO_REQ,
	IRCV3_CAP_ERR_UNSUPPORTED,
	IRCV3_CAP_ERR_WAS_SET,
	IRCV3_CAP_ERR_WAS_UNSET,
};

static struct ircv3_cap* ircv3_cap_get(struct ircv3_caps*, const char*);

void
ircv3_caps(struct ircv3_caps *caps)
{
	#define X(CAP, VAR, ATTRS) \
	caps->VAR.req = 0; \
	caps->VAR.set = 0; \
	caps->VAR.req_auto = (ATTRS & IRCV3_CAP_AUTO);
	IRCV3_CAPS
	#undef X

	caps->cap_reqs = 0;
}

void
ircv3_caps_reset(struct ircv3_caps *caps)
{
	#define X(CAP, VAR, ATTRS) \
	caps->VAR.req = 0; \
	caps->VAR.set = 0;
	IRCV3_CAPS
	#undef X

	caps->cap_reqs = 0;
}

int
ircv3_cap_ack(struct ircv3_caps *caps, const char *cap_str)
{
	int unset;
	struct ircv3_cap *cap;

	if ((unset = (*cap_str == '-')))
		cap_str++;

	if (!(cap = ircv3_cap_get(caps, cap_str)))
		return IRCV3_CAP_ERR_UNSUPPORTED;

	if (!cap->req)
		return IRCV3_CAP_ERR_NO_REQ;

	cap->req = 0;

	if (!unset && cap->set)
		return IRCV3_CAP_ERR_WAS_SET;

	if (unset && !cap->set)
		return IRCV3_CAP_ERR_WAS_UNSET;

	cap->set = !unset;

	if (caps->cap_reqs)
		caps->cap_reqs--;

	return IRCV3_CAP_ERR_NONE;
}

int
ircv3_cap_nak(struct ircv3_caps *caps, const char *cap_str)
{
	int unset;
	struct ircv3_cap *cap;

	if ((unset = (*cap_str == '-')))
		cap_str++;

	if (!(cap = ircv3_cap_get(caps, cap_str)))
		return IRCV3_CAP_ERR_UNSUPPORTED;

	if (!cap->req)
		return IRCV3_CAP_ERR_NO_REQ;

	cap->req = 0;

	if (!unset && cap->set)
		return IRCV3_CAP_ERR_WAS_SET;

	if (unset && !cap->set)
		return IRCV3_CAP_ERR_WAS_UNSET;

	if (caps->cap_reqs)
		caps->cap_reqs--;

	return IRCV3_CAP_ERR_NONE;
}

const char*
ircv3_cap_err(int err)
{
	switch (err) {
		case IRCV3_CAP_ERR_NO_REQ:      return "no cap REQ";
		case IRCV3_CAP_ERR_UNSUPPORTED: return "not supported";
		case IRCV3_CAP_ERR_WAS_SET:     return "was set";
		case IRCV3_CAP_ERR_WAS_UNSET:   return "was unset";
		default:                        return "unknown error";
	}
}

static struct ircv3_cap*
ircv3_cap_get(struct ircv3_caps *caps, const char *cap_str)
{
	#define X(CAP, VAR, ATTRS) \
	if (!strcmp(cap_str, CAP)) \
		return &(caps->VAR);
	IRCV3_CAPS
	#undef X

	return NULL;
}

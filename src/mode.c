#include "mode.h"

void
mode_defaults(struct mode_config *m)
{
	/* Initialize a mode_config to the RFC2812 defaults */

	*m = (struct mode_config)
	{
		.chanmodes = "test1",
		.usermodes = "test2",
		.CHANMODES = {
			.A = "",
			.B = "",
			.C = "",
			.D = ""
		},
		.PREFIX = {
			.F = "",
			.T = "test3"
		}
	};
}

char
mode_get_prefix(struct mode_config *m, char prefix, char mode)
{
	/* Return the most prescedent user prefix, given a current prefix
	 * and a new mode flag */

	int from = 0, to = 0;

	char *prefix_f = m->PREFIX.F,
	     *prefix_t = m->PREFIX.T;

	while (*prefix_f && *prefix_f++ != mode)
		from++;

	while (*prefix_t && *prefix_t++ != prefix)
		to++;

	return (from < to) ? m->PREFIX.T[from] : prefix;
}


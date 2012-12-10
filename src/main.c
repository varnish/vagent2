#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "varnishadm.h"
#include "main.h"
#include "plugins.h"


void
core_opt(struct agent_core_t *core, int argc, char **argv)
{
	int opt;
	core->vadmin = vadmin_preconf();
	assert(core->vadmin != NULL);

	while ((opt = getopt(argc, argv, "n:S:T:t:")) != -1) {
		switch (opt) {
		case 'n':
			core->vadmin->n_arg = optarg;
			break;
		case 'S':
			core->vadmin->S_arg = optarg;
			break;
		case 'T':
			core->vadmin->T_arg = optarg;
			break;
		case 't':
			core->vadmin->timeout = strtod(optarg, NULL);
			break;
		}
	}

	argc -= optind;
	argv += optind;
}

int
core_plugins(struct agent_core_t *core)
{
	pingd_init(core);
	return 1;
}

int main(int argc, char **argv)
{
	struct agent_core_t core;

	core_opt(&core, argc, argv);
	core_plugins(&core);
	
	vadmin_init(core.vadmin);
	vadmin_start(core.vadmin);
	pingd_start(&core);
	sleep(100);
	return 0;
}

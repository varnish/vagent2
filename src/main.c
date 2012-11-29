#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "varnishadm.h"

struct agent_core_t {
	struct vadmin_config_t *vadmin;
};


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

int main(int argc, char **argv)
{
	struct vadmin_ret vret;
	struct agent_core_t core;

	core_opt(&core, argc, argv);
	
	vadmin_init(core.vadmin);
	vadmin_run(core.vadmin,"ping",&vret);
	printf("Return: %d msg: %s\n", vret.status, vret.answer);
	vadmin_run(core.vadmin,"help",&vret);
	printf("Return: %d msg: %s\n", vret.status, vret.answer);
	return 0;
}

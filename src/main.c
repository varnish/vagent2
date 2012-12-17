#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "varnishadm.h"
#include "main.h"
#include "plugins.h"


void
core_opt(struct agent_core_t *core, int argc, char **argv)
{
	int opt;
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

static void core_alloc_plugins(struct agent_core_t *core)
{
	struct agent_plugin_t *v;
	
	plugin_alloc("pingd",core);
	plugin_alloc("logd",core);
	plugin_alloc("vadmin",core);

	v  = plugin_find(core,"vadmin");
	vadmin_preconf(core);
	core->vadmin = (struct vadmin_config_t *)v->data;
}

int
core_plugins(struct agent_core_t *core)
{
	pingd_init(core);
	logd_init(core);
	vadmin_init(core);
	return 1;
}

int main(int argc, char **argv)
{
	struct agent_core_t core;
	struct agent_plugin_t *plug;
	core.plugins = NULL;
	core_alloc_plugins(&core);
	core_opt(&core, argc, argv);
	core_plugins(&core);
	for (plug = core.plugins; plug != NULL; plug = plug->next) {
		printf("hopping to %s\n", plug->name);
	}
	for (plug = core.plugins; plug != NULL; plug = plug->next) {
		printf("Starting %s\n", plug->name);
		plug->start(&core, plug->name);
	}

	for (plug = core.plugins; plug; plug = plug->next) {
		if (plug->thread) {
			printf("Waiting for %s\n", plug->name);
			pthread_join(*plug->thread, NULL);
		} else {
			printf("Plugin %s has no threads\n", plug->name);
		}
	}
	return 0;
}

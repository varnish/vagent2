#include "main.h"
#include "plugins.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

struct pingd_priv_t {
	int vadmin_sock;
};

void
pingd_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug = malloc(sizeof(struct agent_plugin_t));
	struct pingd_priv_t *priv = malloc(sizeof(struct pingd_priv_t));
	
	priv->vadmin_sock = vadmin_register(core);	
	plug->name = "pingd";
	plug->data = (void *)priv;
	plug->next = core->plugins;
	core->plugins = plug;
}

void *pingd_run(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct agent_plugin_t *plug;
	struct pingd_priv_t *ping;
	struct vadmin_ret vret;
	
	plug = plugin_find(core,"pingd");
	ping = (struct pingd_priv_t *) plug->data;

	while (1) {
		sleep(1);
		vadmin_run(ping->vadmin_sock, "ping", &vret);
		printf("PING Return: %d msg: %s\n", vret.status, vret.answer);
	}
	return NULL;
}

void pingd_start(struct agent_core_t *core)
{
	pthread_t thread;
	pthread_create(&thread,NULL,(*pingd_run),core);
}

#include "main.h"
#include "plugins.h"
#include "ipc.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

struct pingd_priv_t {
	int vadmin_sock;
};

void *pingd_run(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct agent_plugin_t *plug;
	struct pingd_priv_t *ping;
	struct ipc_ret_t vret;
	
	plug = plugin_find(core,"pingd");
	ping = (struct pingd_priv_t *) plug->data;

	while (1) {
		sleep(1);
		ipc_run(ping->vadmin_sock, "ping", &vret);
		printf("PING Return: %d msg: %s\n", vret.status, vret.answer);
	}
	return NULL;
}
pthread_t *
pingd_start(struct agent_core_t *core, const char *name)
{
	pthread_t *thread = malloc(sizeof (pthread_t));
	pthread_create(thread,NULL,(*pingd_run),core);
	return thread;
}

void
pingd_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct pingd_priv_t *priv = malloc(sizeof(struct pingd_priv_t));
	plug = plugin_find(core,"pingd");
	
	priv->vadmin_sock = ipc_register(core,"vadmin");	
	plug->data = (void *)priv;
	plug->start = pingd_start;
}



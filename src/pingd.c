#include "main.h"
#include "plugins.h"
#include "ipc.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct pingd_priv_t {
	int vadmin_sock;
	int logger;
};

void *pingd_run(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct agent_plugin_t *plug;
	struct pingd_priv_t *ping;
	struct ipc_ret_t vret;
	
	plug = plugin_find(core,"pingd");
	ping = (struct pingd_priv_t *) plug->data;

	logger(ping->logger, "Health check starting at 1 second intervals");
	while (1) {
		sleep(1);
		ipc_run(ping->vadmin_sock, "ping", &vret);
		if (vret.status != 200)
			logger(ping->logger, "Ping failed. %d %s", vret.status, vret.answer);

		ipc_run(ping->vadmin_sock, "status", &vret);
		if (vret.status != 200 || strcmp(vret.answer,"Child in state running"))
			logger(ping->logger, "%d %s", vret.status, vret.answer);
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
	priv->logger = ipc_register(core,"logd");
	plug->data = (void *)priv;
	plug->start = pingd_start;
}



#include "main.h"
#include "plugins.h"
#include "ipc.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <poll.h>
#include <sys/socket.h>

struct logd_priv_t {
	FILE *out;
	struct ipc_t *ipc;
};

void read_log(void *private, char *msg, struct ipc_ret_t *ret)
{
	struct logd_priv_t *log = (struct logd_priv_t *) private;

	fprintf(log->out,"logger: %s\n",msg);
	
	ret->status = 200;
	ret->answer = "OK";
}

void logd_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct logd_priv_t *priv = malloc(sizeof(struct logd_priv_t));
	plug = plugin_find(core,"logd");
	
	priv->out = stdout;
	plug->ipc->cb = read_log;
	plug->ipc->priv = priv;
	plug->data = (void *)priv;
	plug->start = ipc_start;
}

pthread_t *logd_start(struct agent_core_t *core, char *name)
{
	printf("NOT RUN\n");
	return ipc_start(core, name);
}

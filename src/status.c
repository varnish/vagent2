

#include "main.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct status_priv_t {
	int logger;
	int vadmin;
};

unsigned int status_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct status_priv_t *status;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;

	plug = plugin_find(core,"status");
	status = plug->data;
	logger(status->logger, "Responding to request");

	response.status = 200;
	ipc_run(status->vadmin, "status", &vret);
	response.body = vret.answer;
	response.nbody = strlen(vret.answer);
	send_response(request->connection, &response);
	return 0;
}

void
status_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct status_priv_t *priv = malloc(sizeof(struct status_priv_t));
	plug = plugin_find(core,"status");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/status", M_GET, status_reply, core);
}



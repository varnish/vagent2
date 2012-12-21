#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct echo_priv_t {
	int logger;
};

unsigned int echo_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct echo_priv_t *echo;
	struct agent_plugin_t *plug;
	plug = plugin_find(core,"echo");
	echo = plug->data;
	logger(echo->logger, "Responding to request");

	response.status = 200;
	response.body = request->data;
	response.nbody = request->ndata;
	send_response(request->connection, &response);
	return 0;
}

void
echo_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct echo_priv_t *priv = malloc(sizeof(struct echo_priv_t));
	plug = plugin_find(core,"echo");
	
	priv->logger = ipc_register(core,"logd");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/echo", M_POST | M_PUT | M_POST, echo_reply, core);
}



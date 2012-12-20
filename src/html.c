#define _GNU_SOURCE
#include "main.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct html_priv_t {
	int logger;
};

unsigned int html_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct html_priv_t *html;
	struct agent_plugin_t *plug;
	int fd, ret;
	char *path;
	char buffer[102400];
	plug = plugin_find(core,"html");
	html = plug->data;
	response.status = 200;
	response.body = "meh";
	response.nbody = strlen(response.body);
	
	fd = asprintf(&path, "html/%s", (strlen(request->url) > strlen("/html/")) ? request->url + strlen("/html/") : "index.html");
	assert(fd>0);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		send_response(request->connection, &response);
		return 0;
	}
		
	ret = read(fd, buffer, 102399);
	assert(ret>0);
	close(fd);
	response.body = buffer;
	response.nbody = ret;
	response.status = 200;

	send_response(request->connection, &response);
	return 0;
}

void
html_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct html_priv_t *priv = malloc(sizeof(struct html_priv_t));
	plug = plugin_find(core,"html");
	
	priv->logger = ipc_register(core,"logd");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/html/", M_GET, html_reply, core);
}



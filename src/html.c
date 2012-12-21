#define _GNU_SOURCE
#include "common.h"
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
	struct httpd_response response;
	int fd, ret;
	char *path;
	char *buffer;
	struct stat sbuf;
	response.status = 404;
	response.body = "meh";
	response.nbody = strlen(response.body);
	
	fd = asprintf(&path, "html/%s", (strlen(request->url) > strlen("/html/")) ? request->url + strlen("/html/") : "index.html");
	assert(fd>0);
	ret = stat(path, &sbuf);
	if (ret < 0) {
		send_response(request->connection, &response);
		return 0;
	}
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		send_response(request->connection, &response);
		return 0;
	}
	buffer = malloc(sbuf.st_size);
	assert(buffer);	
	ret = read(fd, buffer, sbuf.st_size);
	assert(ret>0);
	assert(ret==sbuf.st_size);
	close(fd);
	response.body = buffer;
	response.nbody = ret;
	response.status = 200;
	send_response(request->connection, &response);
	free(buffer);
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



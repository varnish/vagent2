/*
 * Copyright (c) 2012 Varnish Software AS
 * All rights reserved.
 *
 * Author: Kristian Lyngstøl <kristian@bohemians.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE
#include "common.h"
#include "plugins.h"
#include "ipc.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "httpd.h"

#define RCV_BUFFER 2048000

struct httpd_listener {
	char *url;
	unsigned int method;
	unsigned int (*cb)(struct httpd_request *request, void *data);
	void *data;
	struct httpd_listener *next;
};

struct httpd_priv_t {
	int logger;
	char *help_page;
	struct httpd_listener *listener;
};

struct connection_info_struct {
	char answerstring[RCV_BUFFER];
	int progress;
};

static char *make_help(struct httpd_priv_t *http)
{
	char *body;
	char buffer[512];
	struct httpd_listener *listener;
	int ret;
	ret = asprintf(&body,"This is the varnish agent.\n\n"
		"GET requests never modify state\n"
		"POST requests are not idempotent, and can modify state\n"
		"PUT requests are idempotent, and can modify state\n"
		"HEAD requests can be performed on all resources that support GET\n"
		"\n"
		"The following URLs are bound:\n\n"
		);
	assert(ret>0);

	for (listener = http->listener; listener != NULL; listener = listener->next) {
		snprintf(buffer, 512, " - %-20s %-3s %-3s %-4s %s\n",
			listener->url,
			(listener->method & M_GET) ? "GET" : "",
			(listener->method & M_PUT) ? "PUT" : "",
			(listener->method & M_POST) ? "POST" : "",
			(listener->method & M_DELETE) ? "DELETE" : ""
			);
		/*
		 * \0 and newline at the end
		 */
		body = realloc(body, strlen(body) + strlen(buffer) + 2);
		assert(body);
		strcat(body,buffer);
	}
	strcat(body,"\n");
	return body;
}

int send_response(struct MHD_Connection *connection, int status, const char *data, unsigned int ndata)
{
	int ret;
	struct MHD_Response *MHDresponse;
	void *data_copy;
	data_copy = malloc(ndata);
	memcpy(data_copy, data, ndata);
	MHDresponse = MHD_create_response_from_data(ndata,
			data_copy, MHD_YES, MHD_NO);
	assert(MHDresponse);

	ret = MHD_queue_response (connection, status, MHDresponse);
	assert(ret == 1);
	MHD_destroy_response (MHDresponse);
	return ret;
}

int send_response_ok(struct MHD_Connection *connection, const char *data)
{
	assert(data);
	return send_response(connection, 200, data, strlen(data));
}

int send_response_fail(struct MHD_Connection *connection, const char *data)
{
	assert(data);
	return send_response(connection, 500, data, strlen(data));
}

static void request_completed (void *cls, struct MHD_Connection *connection,
		   void **con_cls, enum MHD_RequestTerminationCode toe)
{
	(void)cls;
	(void)connection;
	(void)toe;
	struct connection_info_struct *con_info =
		(struct connection_info_struct *) *con_cls;


	if (!con_info)
		return;

	free(con_info);
	*con_cls = NULL;
}

static int find_listener(struct httpd_request *request, struct httpd_priv_t *http)
{
	struct httpd_listener *listener;
	assert(request);
	for (listener = http->listener; listener != NULL; listener = listener->next) {
		if (!strncmp(listener->url,request->url,strlen(listener->url))) {
			if (!(listener->method & request->method))
				return 0;
			listener->cb(request, listener->data);
			return 1;
		}
	}
	return 0;
}

static int answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
	struct agent_core_t *core = (struct agent_core_t *)cls;
	struct httpd_priv_t *http;
	struct agent_plugin_t *plug;
	struct httpd_request request;
	struct connection_info_struct *con_info = NULL;

	(void)version;
	plug = plugin_find(core,"httpd");
	http = (struct httpd_priv_t *) plug->data;
	assert(plug);
	assert(http);


	if (NULL == *con_cls) {
		con_info = malloc (sizeof (struct connection_info_struct));
		assert(con_info);
		con_info->answerstring[0] = '\0';
		con_info->progress = 0;
		*con_cls = con_info;
		return MHD_YES;
	}
	logger(http->logger, "%s %s",method, url );
	if (0 == strcmp (method, "GET") || !strcmp(method, "HEAD") || !strcmp(method,"DELETE")) {
		if (!strcmp(method,"DELETE")) {
			request.method = M_DELETE;
		} else {
			request.method = M_GET;
		}
		request.connection = connection;
		request.url = url;
		request.ndata = 0;
		if (find_listener(&request, http))
			return MHD_YES;
	}


	if (!strcmp(method, "POST") || !strcmp(method, "PUT")) {
		assert(con_info == NULL);
		con_info = *con_cls;

		if (*upload_data_size != 0) {
			if (*upload_data_size + con_info->progress >= RCV_BUFFER)
				return send_response_fail (connection,"Buffer exceeded");
			memcpy(con_info->answerstring + con_info->progress,
				upload_data, *upload_data_size);
			con_info->progress += *upload_data_size;
			*upload_data_size = 0;

			return MHD_YES;
		} else if (NULL != con_info->answerstring){
			if (!strcmp(method,"POST")) {
				request.method = M_POST;
			} else {
				request.method = M_PUT;
			}
			request.connection = connection;
			request.url = url;
			request.ndata = con_info->progress;
			request.data = con_info->answerstring;
			/*
			 * FIXME
			 */
			((char *)request.data)[con_info->progress] = '\0';
			if (find_listener(&request, http))
				return MHD_YES;
		}
	}
	if (request.method == M_GET && !strcmp(url, "/")) {
		if (http->help_page == NULL)
			http->help_page = make_help(http);
		assert (http->help_page);
		return send_response_ok(connection, http->help_page);
	}


	return send_response_fail (connection, "Failed\n");
}

static void *httpd_run(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct agent_plugin_t *plug;
	struct httpd_priv_t *http;
	int port;
	struct MHD_Daemon *d;
	assert(data);
	assert(core);
	plug = plugin_find(core,"httpd");
	assert(plug);
	http = (struct httpd_priv_t *) plug->data;
	assert(http);
	port = atoi(core->config->c_arg);
	assert(port > 0);
	logger(http->logger, "HTTPd starting");
	d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, port, NULL, NULL,
                             &answer_to_connection, data,
                             MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                             NULL, MHD_OPTION_END);
	assert(d);
	logger(http->logger,"HTTP started?");

	/*
	 * XXX: .....
	 */
	while (1)
		sleep(100);
	MHD_stop_daemon(d);

	return NULL;
}


int httpd_register_url(struct agent_core_t *core, const char *url,
		       unsigned int method,
		       unsigned int (*cb)(struct httpd_request *request,
		       void *data), void *data)
{
	struct httpd_listener *listener = malloc(sizeof(struct httpd_listener));
	struct agent_plugin_t *plug = plugin_find(core,"httpd");
	struct httpd_priv_t *httpd = plug->data;
	assert(listener);
	assert(plug);
	assert(httpd);
	listener->url = strdup(url);
	assert(listener->url);
	assert(cb);
	listener->method = method;
	listener->cb = cb;
	listener->data = data;
	listener->next = httpd->listener;
	httpd->listener = listener;
	return 1;
}

static pthread_t *httpd_start(struct agent_core_t *core, const char *name)
{
	int ret;
	pthread_t *thread = malloc(sizeof (pthread_t));
	assert(thread);
	ret = pthread_create(thread,NULL,(*httpd_run),core);
	assert(ret == 0);
	(void)name;
	return thread;
}

void httpd_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct httpd_priv_t *priv = malloc(sizeof(struct httpd_priv_t));
	assert(priv);
	plug = plugin_find(core,"httpd");
	assert(plug);
	priv->logger = ipc_register(core,"logd");
	plug->data = (void *)priv;
	plug->start = httpd_start;
	priv->listener = NULL;
	priv->help_page = NULL;
}

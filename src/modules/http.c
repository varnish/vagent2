/*
 * Copyright (c) 2012-2013 Varnish Software AS
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
#include <netinet/in.h>

#include "http.h"
#include "base64.h"

#define RCV_BUFFER 2048000

struct http_listener {
	char *url;
	unsigned int method;
	unsigned int (*cb)(struct http_request *request, void *data);
	void *data;
	struct http_listener *next;
};

struct http_priv_t {
	int logger;
	char *help_page;
	struct http_listener *listener;
};

struct connection_info_struct {
	char answerstring[RCV_BUFFER];
	int progress;
	int authed;
};

static char *make_help(struct http_priv_t *http)
{
	char *body;
	char buffer[512];
	struct http_listener *listener;
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

static int send_auth_response(struct MHD_Connection *connection)
{
	struct http_response *resp = http_mkresp(connection, 401, "Authorize, please");
	http_add_header(resp, "WWW-Authenticate", "Basic realm=varnish-agent");
	send_response2(resp);
	http_free_resp(resp);
	return 1;
}

void http_add_header(struct http_response *resp, const char *key, const char *value)
{
	struct http_header *hdr = malloc(sizeof(struct http_header));
	assert(hdr);
	assert(key);
	assert(value);
	hdr->key = strdup(key);
	hdr->value = strdup(value);
	assert(hdr->key);
	assert(hdr->value);
	hdr->next = resp->headers;
	resp->headers = hdr;
}


void http_free_resp(struct http_response *resp)
{
	struct http_header *hdr;
	struct http_header *to_delete;
	for (hdr = resp->headers; hdr; ) {
		free(hdr->key);
		free(hdr->value);
		to_delete = hdr;
		hdr = hdr->next;
		free(to_delete);
	}
	free(resp);
}

struct http_response *http_mkresp(struct MHD_Connection *conn, int status, const char *body)
{
	struct http_response *resp = malloc(sizeof(struct http_response));
	resp->status = status;
	resp->connection = conn;
	resp->data = body;
	if (!resp->data)
		resp->ndata = 0;
	else
		resp->ndata = strlen(resp->data);
	resp->headers = NULL;
	return resp;
}

int send_response2(struct http_response *resp)
{
	int ret;
	struct MHD_Response *MHDresponse;
	void *data_copy;
	struct http_header *hdr;
	data_copy = malloc(resp->ndata);
	memcpy(data_copy, resp->data, resp->ndata);
	MHDresponse = MHD_create_response_from_data(resp->ndata,
			data_copy, MHD_YES, MHD_NO);
	assert(MHDresponse);
	for (hdr = resp->headers; hdr; hdr = hdr->next)
		MHD_add_response_header(MHDresponse, hdr->key, hdr->value);

	ret = MHD_queue_response (resp->connection, resp->status, MHDresponse);
	assert(ret == 1);
	MHD_destroy_response (MHDresponse);
	return ret;
}

static int send_response(struct MHD_Connection *connection, int status, const char *data, unsigned int ndata)
{
	struct http_response *resp = http_mkresp(connection, status, NULL);
	int ret;
	resp->data = data;
	resp->ndata = ndata;
	ret = send_response2(resp);
	http_free_resp(resp);
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

static int find_listener(struct http_request *request, struct http_priv_t *http)
{
	struct http_listener *listener;
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

struct header_finder_t {
	const char *header;
	char *value;
};

static int get_key (void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	struct header_finder_t *finder = cls;
	(void)kind;
	if (!strcasecmp(finder->header, key)) {
		finder->value = strdup(value);
		return MHD_NO;
	}
	return MHD_YES;
}

static char *http_get_header(struct MHD_Connection *connection, const char *header)
{
	struct header_finder_t finder;
	finder.header = header;
	finder.value = NULL;
	MHD_get_connection_values (connection, MHD_HEADER_KIND, &get_key, &finder);
	return finder.value;
}

static void log_request(struct MHD_Connection *connection,
			const struct http_priv_t *http,
			const char *method,
			const char *url)
{


#if MHD_VERSION < 0x00090600
	const union MHD_ConnectionInfo *info;
	const unsigned char *ip;
	info = MHD_get_connection_info (connection,
			MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	assert(info);

	ip = (unsigned char *)&info->client_addr->sin_addr.s_addr;
	logger(http->logger, "%hhu.%hhu.%hhu.%hhu:%d - %s %s",
	       ip[0], ip[1], ip[2], ip[3], info->client_addr->sin_port,
	       method, url);
#else
	(void)connection;
	logger(http->logger, "%s %s", method, url);
#endif
}

static int check_auth(struct MHD_Connection *connection, struct agent_core_t *core, struct connection_info_struct *con_info)
{
	_cleanup_free_ char *auth = NULL;
	char base64pw[1024];
	assert(con_info);
	if (con_info->authed == 0) {
		auth = http_get_header(connection, "Authorization");
		/*
		 * XXX: Should be stored somewhere...
		 */
		base64_encode(BASE64, core->config->userpass, strlen(core->config->userpass), base64pw, sizeof(base64pw));
		if (!auth || strncmp(auth,"Basic ", strlen("Basic ")) || strcmp(auth + strlen("Basic "), base64pw)) {
			send_auth_response(connection);
			return 1;
		}
		con_info->authed = 1;
	}
	return 0;
}

static int answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
	struct agent_core_t *core = (struct agent_core_t *)cls;
	struct http_priv_t *http;
	struct agent_plugin_t *plug;
	struct http_request request;
	struct connection_info_struct *con_info = NULL;
	int ret;

	(void)version;
	plug = plugin_find(core,"http");
	http = (struct http_priv_t *) plug->data;
	assert(plug);
	assert(http);


	if (NULL == *con_cls) {
		con_info = malloc (sizeof (struct connection_info_struct));
		assert(con_info);
		con_info->answerstring[0] = '\0';
		con_info->progress = 0;
		con_info->authed = 0;
		*con_cls = con_info;
		return MHD_YES;
	}
	con_info = *con_cls;
	assert(core->config->userpass);

	log_request(connection, http, method, url);

	if (0 == strcmp (method, "GET") || !strcmp(method, "HEAD") || !strcmp(method,"DELETE")) {
		ret = check_auth(connection, core, con_info);
		if (ret == 1)
			return MHD_YES;
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

		if (*upload_data_size != 0) {
			if (*upload_data_size + con_info->progress >= RCV_BUFFER) {
				warnlog(http->logger, "Client input exceeded buffer size of %u bytes. Dropping client.", RCV_BUFFER);

				 return MHD_NO;
			}
			memcpy(con_info->answerstring + con_info->progress,
				upload_data, *upload_data_size);
			con_info->progress += *upload_data_size;
			*upload_data_size = 0;

			return MHD_YES;
		} else if (NULL != con_info->answerstring){
			ret = check_auth(connection, core, con_info);
			if (ret == 1)
				return MHD_YES;
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

static void *http_run(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct agent_plugin_t *plug;
	struct http_priv_t *http;
	int port;
	struct MHD_Daemon *d;
	assert(data);
	assert(core);
	plug = plugin_find(core,"http");
	assert(plug);
	http = (struct http_priv_t *) plug->data;
	assert(http);
	port = atoi(core->config->c_arg);
	assert(port > 0);
	logger(http->logger, "HTTPd starting on port %i",port);
	d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, port, NULL, NULL,
                             &answer_to_connection, data,
                             MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                             NULL, MHD_OPTION_END);
	if (!d) {
		warnlog(http->logger, "HTTP failed to start on port %d. Agent already running?",port);
		sleep(1);
		exit(1);
	}

	/*
	 * XXX: .....
	 */
	while (1)
		sleep(100);
	MHD_stop_daemon(d);

	return NULL;
}


int http_register_url(struct agent_core_t *core, const char *url,
		       unsigned int method,
		       unsigned int (*cb)(struct http_request *request,
		       void *data), void *data)
{
	struct http_listener *listener = malloc(sizeof(struct http_listener));
	struct agent_plugin_t *plug = plugin_find(core,"http");
	struct http_priv_t *http = plug->data;
	assert(listener);
	assert(plug);
	assert(http);
	listener->url = strdup(url);
	assert(listener->url);
	assert(cb);
	listener->method = method;
	listener->cb = cb;
	listener->data = data;
	listener->next = http->listener;
	http->listener = listener;
	return 1;
}

static pthread_t *http_start(struct agent_core_t *core, const char *name)
{
	int ret;
	pthread_t *thread = malloc(sizeof (pthread_t));
	assert(thread);
	ret = pthread_create(thread,NULL,(*http_run),core);
	assert(ret == 0);
	(void)name;
	return thread;
}

void http_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct http_priv_t *priv = malloc(sizeof(struct http_priv_t));
	assert(priv);
	plug = plugin_find(core,"http");
	assert(plug);
	priv->logger = ipc_register(core,"logger");
	plug->data = (void *)priv;
	plug->start = http_start;
	priv->listener = NULL;
	priv->help_page = NULL;
}

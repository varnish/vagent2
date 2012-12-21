#define _GNU_SOURCE
#include "main.h"
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
	char answerstring[2048];
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
		snprintf(buffer, 512, " - %s ",listener->url);
		if (listener->method & M_GET)
			strncat(buffer, "GET ", 511);
		if (listener->method & M_PUT)
			strncat(buffer, "PUT ", 511);
		if (listener->method & M_POST)
			strncat(buffer, "POST ", 511);
		strncat(buffer,"\n",511);
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
int send_response(struct MHD_Connection *connection, struct httpd_response *response)
{
	int ret;
	struct MHD_Response *MHDresponse;
	MHDresponse = MHD_create_response_from_data(response->nbody,
			response->body, MHD_NO, MHD_YES);
	assert(MHDresponse);
	if (!MHDresponse)
		return MHD_NO;

	ret = MHD_queue_response (connection, response->status, MHDresponse);
	assert(ret == 1);
	MHD_destroy_response (MHDresponse);

	return ret;
}

static void request_completed (void *cls, struct MHD_Connection *connection,
		   void **con_cls, enum MHD_RequestTerminationCode toe)
{
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

static int
answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
	struct agent_core_t *core = (struct agent_core_t *)cls;
	struct httpd_priv_t *http;
	struct agent_plugin_t *plug;
	struct httpd_response bad_response;
	struct httpd_request request;
	
	plug = plugin_find(core,"httpd");
	http = (struct httpd_priv_t *) plug->data;

	logger(http->logger, "Got connection on url %s",url );

	if (NULL == *con_cls) {
		struct connection_info_struct *con_info;
		con_info = malloc (sizeof (struct connection_info_struct));
		assert(con_info);
		con_info->answerstring[0] = '\0';
		con_info->progress = 0;
		*con_cls = con_info;
		return MHD_YES;
	}
	bad_response.status = 400;
	bad_response.body = "Unknown request\n";
	bad_response.nbody = strlen(bad_response.body);
	if (0 == strcmp (method, "GET") || !strcmp(method, "HEAD")) {
		request.method = M_GET;
		request.connection = connection;
		request.url = url;
		request.ndata = 0;
		if (find_listener(&request, http))
			return MHD_YES;
		bad_response.status = 404;
	}


	if (!strcmp(method, "POST") || !strcmp(method, "PUT")) {
		struct connection_info_struct *con_info = *con_cls;

		if (*upload_data_size != 0) {
			if (*upload_data_size + con_info->progress >= 2048)
				return send_response (connection, &bad_response);
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
		bad_response.status = 200;
		bad_response.body = http->help_page;
		bad_response.nbody = strlen(bad_response.body);
	}


	return send_response (connection, &bad_response);
}

void *httpd_run(void *data)
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


int httpd_register_url(struct agent_core_t *core, char *url,
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
pthread_t *
httpd_start(struct agent_core_t *core, const char *name)
{
	int ret;
	pthread_t *thread = malloc(sizeof (pthread_t));
	assert(thread);
	ret = pthread_create(thread,NULL,(*httpd_run),core);
	assert(ret == 0);
	return thread;
}

void
httpd_init(struct agent_core_t *core)
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



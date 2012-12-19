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

#define PAGE "<html><head><title>libmicrohttpd demo</title>"\
             "</head><body>libmicrohttpd demo</body></html>"
#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512
#define GET             0
#define POST            1

struct httpd_listener {
	char *url;
	unsigned int method;
	unsigned int (*cb)(struct httpd_request *request, void *data);
	void *data;
	struct httpd_listener *next;
};

struct httpd_priv_t {
	int logger;
	int context;
	struct httpd_listener *listener;
};

struct connection_info_struct {
	int connectiontype;
	char answerstring[2048];
	int progress;
};

int send_response(struct MHD_Connection *connection, struct httpd_response *response)
{
	int ret;
	struct MHD_Response *MHDresponse;
	MHDresponse = MHD_create_response_from_data(response->nbody,
			response->body, MHD_NO, MHD_NO);
	if (!MHDresponse)
		return MHD_NO;

	ret = MHD_queue_response (connection, response->status, MHDresponse);
	MHD_destroy_response (MHDresponse);

	return ret;
}

static void request_completed (void *cls, struct MHD_Connection *connection,
		   void **con_cls, enum MHD_RequestTerminationCode toe)
{
	struct connection_info_struct *con_info =
		(struct connection_info_struct *) *con_cls;


	if (NULL == con_info)
		return;

	if (con_info->connectiontype == POST) {
		if (con_info->answerstring)
			free (con_info->answerstring);
	}

	free (con_info);
	*con_cls = NULL;
}

static int find_listener(struct httpd_request *request, struct httpd_priv_t *http)
{
	struct httpd_listener *listener;
	for (listener = http->listener; listener != NULL; listener = listener->next) {
		if (!strncmp(listener->url,request->url,strlen(listener->url))) {
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
	
	plug = plugin_find(core,"httpd");
	http = (struct httpd_priv_t *) plug->data;

	logger(http->logger, "Got connection");

	if (NULL == *con_cls) {
		struct connection_info_struct *con_info;
		con_info = malloc (sizeof (struct connection_info_struct));
		if (NULL == con_info)
			return MHD_NO;
		con_info->answerstring[0] = '\0';
		con_info->progress = 0;
		*con_cls = con_info;
		return MHD_YES;
	}
	struct httpd_response bad_response;
	bad_response.status = 500;
	bad_response.body = "Bad response\n";
	bad_response.nbody = strlen(bad_response.body);
	if (0 == strcmp (method, "GET")) {
		struct httpd_request request;
		request.method = M_GET;
		request.connection = connection;
		request.url = url;
		request.ndata = 0;
		if (find_listener(&request, http))
			return MHD_YES;
	}


	if (0 == strcmp (method, "POST")) {
		struct connection_info_struct *con_info = *con_cls;

		if (*upload_data_size != 0) {
			if (*upload_data_size + con_info->progress >= 2048)
				return send_response (connection, &bad_response);
			printf("POST upload-data ikke 0\n");
			memcpy(con_info->answerstring + con_info->progress,
				upload_data, *upload_data_size);
			con_info->progress += *upload_data_size;
			*upload_data_size = 0;

			return MHD_YES;
		} else if (NULL != con_info->answerstring){
			struct httpd_request request;
			request.method = M_POST;
			request.connection = connection;
			request.url = url;
			request.ndata = con_info->progress;
			request.data = con_info->answerstring;
			if (find_listener(&request, http))
				return MHD_YES;
		}
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
}



#ifndef HTTPD_H
#define HTTPD_H


enum http_method {
	M_GET = 1,
	M_POST = 2,
	M_PUT = 4
};

#define METHOD_BITS(s) (1<<s)

struct httpd_request {
	struct MHD_Connection *connection;
	enum http_method method;
	const char *url;
	char **headers;
	unsigned int nheaders;
	void *data;
	unsigned int ndata;
};

struct httpd_response {
	unsigned int status;
	void *body;
	unsigned int nbody;
};

int send_response(struct MHD_Connection *connection, struct httpd_response *response);

/*
 * URL    - the HTTP-protocol URL (e.g: req.url, not including host-header).
 * method - a bitmap of which methods to care for (e.g: 1 for just GET, 3
 *          for GET+POST, etc. Other methods will cause a 405 response.
 * cb     - the callback executed when a matching request is received.
 * data   - arbitrary private data associated with the callback.
 *
 * Returns true on success and false if a callback is already registered
 * for that URL.
 *
 * Note that you can not have multiple listeners for the same URL even if
 * they don't have overlapping methods.
 */
int httpd_register_url(struct agent_core_t *core, char *url,
		       unsigned int method,
		       unsigned int (*cb)(struct httpd_request *request,
		       void *data), void *data);

#endif

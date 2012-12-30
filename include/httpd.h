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

#ifndef HTTPD_H
#define HTTPD_H


/*
 * Methods (duh).
 *
 * Keep in mind:
 * GET should never affect state.
 * POST can affect state and does not require idempotence.
 * PUT can affect state but has to be idempotent.
 *
 * HEAD is the same as GET, the httpd-module handles it transparently for
 * us.
 *
 * Examples:
 *
 * GET /vcl/ - fetches list of VCL
 * POST /vcl/ - uploads a VCL to a dynamic url, multiple uploads leads to
 *              multiple copies.
 * PUT /vcl/ - Error
 * PUT /vcl/name - uploads a VCL to the name specified. Multiple uploads
 *                 will try to overwrite the VCL (which fails, since
 *                 Varnish disallows this)
 * PUT /stop - stops varnish (Can't stop it multiple times now, can we?)
 *
 * etc.
 */
enum http_method {
	M_GET = 1,
	M_POST = 2,
	M_PUT = 4,
	M_DELETE = 8
};

/*
 * HTTP rquest passed to the cb.
 *
 * connection is the connection handle used by MHD.
 * data is the actual data, not guaranteed to be nul-terminated.
 * ndata is the length of data. can be 0.
 */
struct httpd_request {
	struct MHD_Connection *connection;
	enum http_method method;
	const char *url;
	char **headers;
	unsigned int nheaders;
	void *data;
	unsigned int ndata;
};

/*
 * Use send_response during a callback to .... send a response.
 *
 * Note that this technically /queues/ the response, which means that the
 * client might not receive the data right away.
 */
int send_response(struct MHD_Connection *connection, int status, void *data, unsigned int ndata);

/*
 * Various shortcuts for send_response().
 */

int send_response_ok(struct MHD_Connection *connection, char *data);
int send_response_fail(struct MHD_Connection *connection, char *data);

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

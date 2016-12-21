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

/*
 * Mostly just a demo/test plugin. Read for profit.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "http.h"
#include "ipc.h"
#include "plugins.h"

struct echo_priv_t {
	int logger;
};

static unsigned int
echo_reply(struct http_request *request, const char *arg, void *data)
{
	struct echo_priv_t *echo = data;
	
	(void)arg;

	if (request->method == M_PUT || request->method == M_POST) {
		if (((char *)request->data)[request->ndata] == '\0' &&
		    strlen((char *)request->data) == request->ndata)
			debuglog(echo->logger, "Data being printed: \n%s",
			    (char *)request->data);
	}
	logger(echo->logger, "Responding to request");
	http_reply_len(request->connection, 200, request->data,
	    request->ndata);
	return (0);
}

void
echo_init(struct agent_core_t *core)
{
	struct echo_priv_t *priv;
	struct agent_plugin_t *plug;

	/*
	 * Allocate the private data structure we'll keep using.
	 */
	ALLOC_OBJ(priv);

	/*
	 * Find our pre-allocated data structure. This is only used to
	 * define start-functions (which we don't have), module-specific
	 * private data and an IPC for the module (which we don't use).
	 */
	plug = plugin_find(core, "echo");

	/*
	 * Register with the logger.
	 */
	priv->logger = ipc_register(core, "logger");

	/*
	 * Store our private data somewhere we can reach it, and set our
	 * start function to NULL since echo is only triggered by HTTP
	 * requests.
	 */
	plug->data = (void *)priv;

	/*
	 * Register the url /echo for the methods POST, PUT and GET. When a
	 * request like that is encountered, the echo_reply function will
	 * be called with "priv" as the last argument.
	 */
	http_register_path(core, "/echo", M_POST | M_PUT | M_GET,
	    echo_reply, priv);
}

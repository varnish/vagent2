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
#include "httpd.h"

#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct params_priv_t {
	int logger;
	int vadmin;
};

static void run_and_respond(int vadmin, struct MHD_Connection *conn, const char *fmt, ...)
{
	struct ipc_ret_t vret;
	va_list ap;
	char *buffer;
	int iret;
	char *ans;


	va_start(ap, fmt);
	iret = vasprintf(&buffer, fmt, ap);
	assert(iret>0);
	va_end(ap);
	ipc_run(vadmin, &vret, buffer);
	free(buffer);
	ans = vret.answer;

	if (vret.status == 200)
		send_response_ok(conn, ans);
	else
		send_response_fail(conn, ans);
}

unsigned int params_reply(struct httpd_request *request, void *data)
{
	const char *arg;
	struct agent_core_t *core = data;
	struct agent_plugin_t *plug;
	struct params_priv_t *params;
	char *body;
	plug = plugin_find(core,"params");
	params = plug->data;
	

	if (request->method == M_GET) {
		if (!strcmp(request->url,"/param/")) {
			run_and_respond(params->vadmin,
				request->connection,
				"param.show");
			return 1;
		} else {
			arg = request->url + strlen("/param/");
			assert(arg && *arg);
			run_and_respond(params->vadmin,
				request->connection,
				"param.show %s", arg);
			return 1;
		}
	} else if (request->method == M_PUT) {
		char *mark;
	 	assert(((char *)request->data)[request->ndata] == '\0');
	 	body = strdup(request->data);
		mark = index(body,'\n');
		if (mark)
			*mark = '\0';
		if (!strcmp(request->url, "/param/")) {
			run_and_respond(params->vadmin,
				request->connection,
				"param.set %s",body);
			free(body);
			return 1;
		} else {
			arg = request->url + strlen("/param/");
			assert(arg && *arg);
			run_and_respond(params->vadmin,
				request->connection,
				"param.set %s %s",arg, body);
			free(body);
			return 1;
		}

	}
	send_response_fail(request->connection, "Failed");
	return 1;
}

void
params_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct params_priv_t *priv = malloc(sizeof(struct params_priv_t));
	plug = plugin_find(core,"params");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/param/", M_PUT | M_GET, params_reply, core);
}

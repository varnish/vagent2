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


#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct params_priv_t {
	int logger;
	int vadmin;
};

static unsigned int run_cmd(struct httpd_request *request, void *data, const char *cmd, const char *arg)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct params_priv_t *params;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;

	plug = plugin_find(core,"params");
	params = plug->data;
	logger(params->logger, "Responding to request");
	assert(cmd);
	assert(params);
	response.status = 200;
	if (arg == NULL)
		ipc_run(params->vadmin, &vret, cmd);
	else
		ipc_run(params->vadmin, &vret, "%s %s", cmd, arg);
	response.body = vret.answer;
	response.nbody = strlen(vret.answer);
	send_response(request->connection, &response);
	return 0;
}
unsigned int params_reply(struct httpd_request *request, void *data)
{
	const char *arg;
	if (request->method == M_GET) {
		if (!strcmp(request->url,"/param/")) {
			run_cmd(request,data,"param.show", NULL);
		} else {
			arg = request->url + strlen("/param/");
			assert(arg && *arg);
			run_cmd(request,data, "param.show",arg);
		}
	}
	return 0;
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

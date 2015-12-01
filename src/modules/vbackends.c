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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "http.h"
#include "helpers.h"
#include "ipc.h"
#include "plugins.h"

#define BACKENDS_HELP \
"VARNISH REALLY ROCKS \n"

struct vbackends_priv_t {
	int logger;
	int vadmin;
};

struct param_opt {
	char *name;
	char *value;
	char *unit;
	char *def;
	char *description;
	struct param_opt *next;
};

static char *vbackends_show_json(char *raw)
{
	int state;
	char *out = NULL, *out2 = NULL, *out3 = NULL;

	state = asprintf(&out3, "{\n");
	state = asprintf(&out, "some json output to be done-->nightmare\n");
	state= asprintf(&out2, "%s %s %s\n}\n", out3, out, raw);
	free(out3);

	if(state)
		return out2;
	return out2;
}

static void backends_json(struct http_request *request,
    struct vbackends_priv_t *vbackends)
{
	struct ipc_ret_t vret;
	char *tmp;
	ipc_run(vbackends->vadmin, &vret, "backend.list");
	if (vret.status == 200) {
		tmp = vbackends_show_json(vret.answer);
		struct http_response *resp = http_mkresp(request->connection, 200, tmp);
		http_add_header(resp,"Content-Type","application/json");
		send_response(resp);
		free(tmp);
		http_free_resp(resp);
	} else {
		send_response_fail(request->connection, vret.answer);
	}
	free(vret.answer);
}

static unsigned int vbackends_reply(struct http_request *request, void *data)
{
	const char *arg;
	struct agent_core_t *core = data;
	struct agent_plugin_t *plug;
	struct vbackends_priv_t *vbackends;
	char *body;

	plug = plugin_find(core,"vbackends");
	vbackends = plug->data;

	if (!strcmp(request->url, "/backendsjson/") &&
	    request->method == M_GET) {
		backends_json(request, vbackends);
		return 1;
	}
	if (request->method == M_PUT) {
		char *mark;
		assert(((char *)request->data)[request->ndata] == '\0');
		body = strdup(request->data);
		mark = strchr(body,'\n');
		if (mark)
			*mark = '\0';
		if (!strcmp(request->url, "/admin_health/")) {
			run_and_respond(vbackends->vadmin,
				request->connection,
				"backend.set_health %s",body);
		} else {
			arg = request->url + strlen("/admin_health/");
			assert(arg && *arg);
			run_and_respond(vbackends->vadmin,
				request->connection,
				"backend.set_health %s %s",arg, body);
		}
		free(body);
		return 1;

	}

	send_response_fail(request->connection, "Failed");
	return 1;
}

void
vbackends_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vbackends_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vbackends");

	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	http_register_url(core, "/admin_health/", M_PUT, vbackends_reply, core);
	http_register_url(core, "/backendjson/", M_GET, vbackends_reply, core);
	http_register_url(core, "/backend/", M_GET, help_reply, strdup(BACKENDS_HELP));
}

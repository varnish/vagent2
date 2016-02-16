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
#include "vsb.h"

#define BACKENDS_HELP \
"GET /backendsjson/ - fetches a list of backends and values\n" \
"PUT /backend/foo - Takes a single value as input (e.g: 1000) and let you change the admin health value\n" \
"For more trick go to the HTML backend page\n"

char* format_line(char*);

struct vbackends_priv_t {
	int logger;
	int vadmin;
};

struct backend_opt {
	char *name;
	char *ref;
	char *admin;
	char *probe;
};

char *format_line(char* line)
{
	struct backend_opt backend = {};
	char *out = NULL, *ptr;
	int state = 0;

	backend.name = strtok_r(line, " ", &ptr);
	backend.ref = strtok_r(NULL, " ",&ptr);
	backend.admin = strtok_r(NULL, " ",&ptr);
	backend.probe =  strtok_r(NULL, "\n",&ptr);

	state = asprintf(&out, 	"\t{\n"
	    "\t\t\"name\": \"%s\",\n"
	    "\t\t\"ref\": \"%s\",\n"
	    "\t\t\"admin\": \"%s\",\n"
	    "\t\t\"probe\": \"%s\"\n"
	    "\t}",
	    backend.name, backend.ref, backend.admin,
	    backend.probe);
	assert(state);
	return out;
}


static char *vbackends_show_json(char *raw)
{
	char  *out1 = NULL,  *out2 = NULL, *out3 = NULL ;
	struct vsb *final= VSB_new_auto();
	char *tokens, *ptr;
	char tmp[1000];
	int raw_len = 0;
	int state = 0;
	int cont = 0;
	int sum = 0;

	raw_len = strlen(raw);
	tokens = strtok(raw, "\n");
	sum = sum + strlen(tokens);

	while(tokens != NULL){
		strcpy(tmp, (tokens));
		tokens = strtok(NULL, "\n");
		sum = sum + strlen(tmp);
		if(cont > 0){
			ptr = format_line(tmp);
			VSB_cat(final, ptr);
			if(sum < raw_len)
				VSB_cat(final, ",\n");
		}
		cont++;
	}

	VSB_finish(final);

	state = asprintf(&out1, "{\n \"backends\" : [\n");
	state = asprintf(&out2, "%s",VSB_data(final));
	state = asprintf(&out3, "%s%s\n]\n}\n", out1, out2);
	VSB_delete(final);

	assert(state);
	return out3;
}

static void backends_json(struct http_request *request,
    struct vbackends_priv_t *vbackends)
{
	struct ipc_ret_t vret;
	char *tmp;
	ipc_run(vbackends->vadmin, &vret, "backend.list");
	if (vret.status == 200) {
		tmp = vbackends_show_json(vret.answer);
		struct http_response *resp = http_mkresp(request->connection,
		    200, tmp);
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

	if (!strcmp(request->url, "/backendjson/") &&
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
		arg = request->url + strlen("/backend/");
		assert(arg && *arg);
		run_and_respond(vbackends->vadmin,
		    request->connection,
		    "backend.set_health %s %s",arg, body);
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
	http_register_url(core, "/backend/", M_PUT, vbackends_reply, core);
	http_register_url(core, "/backendjson/", M_GET, vbackends_reply, core);
	http_register_url(core, "/help/backend", M_GET,
	    help_reply, strdup(BACKENDS_HELP));
}

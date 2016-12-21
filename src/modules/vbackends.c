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
"PUT /backend/foo - Takes a single value as input (e.g: 1000)" \
" and let you change the admin health value\n" \
"For more tricks go to the HTML backend page\n"

struct vbackends_priv_t {
	int logger;
	int vadmin;
};

struct backend_opt {
	char *name;
	char *admin;
	char *probe;
};

static char *
format_line(char *line)
{
	struct backend_opt backend = {};
	char *out = NULL, *ptr = NULL;

	if(line == NULL)
		return (NULL);

	backend.name = strtok_r(line, " ", &ptr);
	backend.admin = strtok_r(NULL, " ",&ptr);
	backend.probe =  strtok_r(NULL, "\n",&ptr);

	AN(asprintf(&out,
	    "\t{\n"
	    "\t\t\"name\": \"%s\",\n"
	    "\t\t\"admin\": \"%s\",\n"
	    "\t\t\"probe\": \"%s\"\n"
	    "\t}",
	    backend.name, backend.admin, backend.probe));
	return (out);
}

static void
vbackends_show_json(struct vsb *json, char *raw)
{
	char *tokens = NULL, *ptr = NULL;
	char tmp[1000];
	int raw_len = 0;
	int cont = 0;
	int sum = 0;

	raw_len = strlen(raw);
	tokens = strtok(raw, "\n");
	sum = sum + strlen(tokens);

	VSB_cat(json, "{\n \"backends\" : [\n");
	while(tokens != NULL){
		strcpy(tmp, (tokens));
		tokens = strtok(NULL, "\n");
		sum = sum + strlen(tmp);
		if(cont > 0){
			ptr = format_line(tmp);
			VSB_cat(json, ptr);
			if(sum < raw_len)
				VSB_cat(json, ",\n");
			free(ptr);
		}
		cont++;
	}

	VSB_cat(json, "\n]\n}\n");
}

static void backends_json(struct http_request *request,
    struct vbackends_priv_t *vbackends)
{
	struct vsb *json;
	struct ipc_ret_t vret;
	ipc_run(vbackends->vadmin, &vret, "backend.list");
	if (vret.status == 200) {
		json = VSB_new_auto();
		assert(json);
		vbackends_show_json(json, vret.answer);
		AZ(VSB_finish(json));
		struct http_response *resp = http_mkresp(request->connection,
		    200, VSB_data(json));
		http_add_header(resp,"Content-Type","application/json");
		send_response(resp);
		VSB_delete(json);
		http_free_resp(resp);
	} else
		http_reply(request->connection, 500, vret.answer);
	free(vret.answer);
}

static unsigned int
vbackends_json_reply(struct http_request *request, const char *arg, void *data)
{
	struct vbackends_priv_t *vbackends;
	struct agent_core_t *core = data;
	struct agent_plugin_t *plug;

	(void)arg;
	plug = plugin_find(core,"vbackends");
	vbackends = plug->data;

	backends_json(request, vbackends);
	return (1);
}

static unsigned int
vbackends_reply(struct http_request *request, const char *arg, void *data)
{
	struct vbackends_priv_t *vbackends;
	struct agent_core_t *core = data;
	struct agent_plugin_t *plug;
	char *body;
	char *mark;

	assert(request->method == M_PUT);

	if (!arg) {
		http_reply(request->connection, 500, "Failed");
		return (1);
	}

	plug = plugin_find(core,"vbackends");
	vbackends = plug->data;

	assert(((char *)request->data)[request->ndata] == '\0');
	body = strdup(request->data);
	mark = strchr(body,'\n');
	if (mark)
		*mark = '\0';
	run_and_respond(vbackends->vadmin, request->connection,
			"backend.set_health %s %s", arg, body);
	free(body);
	return (1);
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
	http_register_path(core, "/backend/", M_PUT, vbackends_reply, core);
	http_register_path(core, "/backendjson/", M_GET,
			vbackends_json_reply, core);
	http_register_path(core, "/help/backend", M_GET,
	    help_reply, strdup(BACKENDS_HELP));
}

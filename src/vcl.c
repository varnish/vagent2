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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

struct vcl_priv_t {
	int logger;
	int vadmin;
	char *help;
};

static void mk_help(struct vcl_priv_t *vcl) {
	vcl->help = "The following logic can be used:\n"
		"GET /vcl/ - Fetch a list of VCLs (e.g: vcl.list)\n"
		"GET /vcl/vclname - Fetch the vcl named vclname (vcl.show)\n"
		"POST /vcl/ - Upload a new VCL, named dynamically. (vcl.inline).\n"
		"PUT /vcl/vclname - Upload a new VCL with the specified name.\n"
		"PUT /vcldiscard/vclname - Discard a named VCL (vcl.discard)\n"
		"PUT /vcldeploy/vclname - Deploy the vcl (e.g: vcl.use)\n";
}

static void vcl_store(struct httpd_request *request,
		      struct vcl_priv_t *vcl,
		      struct ipc_ret_t *vret,
		      struct httpd_response *response,
		      const char *id)
{
	ipc_run(vcl->vadmin, vret, "vcl.inline %s << __EOF_%s__\n%s\n__EOF_%s__",id,id,(char *)request->data,id);
	if (vret->status == 200) {
		response->status = 201;
		logger(vcl->logger, "VCL stored OK");
	} else {
		response->status = 500;
		logger(vcl->logger, "vcl.inline failed");
	}
}


struct vcl_list {
	char name[1024];
	char available[11];
	char ref[11];
};

static char *vcl_list_json(char *raw)
{
	struct vcl_list *tmp;
	int ret, ret2;
	char *pos;
	char *buffer = calloc(1,10241);
	char *b;
	pos = raw;
	tmp = malloc(sizeof (struct vcl_list));
	strcat(buffer,"{\n\"vcls\": [\n");
	do {
		ret = sscanf(pos, "%10s %6s %s\n", tmp->available, tmp->ref, tmp->name);
		assert(ret>0);
		ret2 = asprintf(&b, "%s{\n"
			"\t\"name\": \"%s\",\n"
			"\t\"status\": \"%s\",\n"
			"\t\"refs\": \"%s\"\n"
			"}",pos != raw ? ",\n" : "",
			tmp->name, tmp->available, tmp->ref);
		assert(ret2>0);
		strncat(buffer,b,10240);
		free(b);
		pos = strstr(pos,"\n");
		if (pos == NULL)
			break;
		pos+=1;
		if (pos[0] == '\0' || pos[0] == '\n')
			break;
	} while (1);
	strncat(buffer,"]\n}\n",10240);
	return buffer;
}

static unsigned int vcl_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct vcl_priv_t *vcl;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;
	char *cmd;
	int ret;

	assert(core);

	plug = plugin_find(core,"vcl");
	assert(plug);

	vcl = plug->data;
	assert(vcl);

	response.status = 500;
	vret.answer = "Invalid VCL-request";
	if (request->method == M_GET) {
		if (!strcmp(request->url, "/vcl") || !strcmp(request->url,"/vcl/")) {
			response.status = 200;
			ipc_run(vcl->vadmin, &vret, "vcl.list");
			vcl_list_json(vret.answer);
		} else if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) { 
			ipc_run(vcl->vadmin, &vret, "vcl.show %s", request->url + strlen("/vcl/"));
			response.status = 200;
		} else if(!strncmp(request->url, "/vclhelp", strlen("/vclhelp"))) {
			vret.answer = vcl->help;
		} else if(!strcmp(request->url, "/vcljson/")) {
			response.status = 200;
			ipc_run(vcl->vadmin, &vret, "vcl.list");
			vret.answer = vcl_list_json(vret.answer);
		}

	} else if (request->method == M_POST) {
		response.status = 200;
		ret = asprintf(&cmd, "%d", (unsigned int)time(NULL));
		assert(ret>0);
		vcl_store(request, vcl, &vret, &response, cmd);
		free(cmd);
	} else if (request->method == M_PUT) {
		if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) {
			if (strlen(request->url) >= 6) {
				response.status = 200;
				vcl_store(request, vcl, &vret, &response, request->url + strlen("/vcl/"));
			}
		} else if (!strncmp(request->url, "/vcldeploy/",strlen("/vcldeploy/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.use %s", request->url + strlen("/vcldeploy/"));
			response.status = 200;
		} else if (!strncmp(request->url, "/vcldiscard/", strlen("/vcldiscard/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.discard %s", request->url + strlen("/vcldiscard/"));
			response.status = 200;
		}
	} else {
		response.status = 400;
		vret.answer = "What now...\n";
	}
	response.body = vret.answer;
	response.nbody = strlen(vret.answer);
	send_response(request->connection, &response);
	return 0;
}

void
vcl_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vcl_priv_t *priv = malloc(sizeof(struct vcl_priv_t));
	plug = plugin_find(core,"vcl");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
	httpd_register_url(core, "/vcljson/", M_GET, vcl_reply, core);
        httpd_register_url(core, "/vcl/", M_PUT | M_GET | M_POST, vcl_reply, core);
        httpd_register_url(core, "/vcldeploy/", M_PUT , vcl_reply, core);
        httpd_register_url(core, "/vcldiscard/", M_PUT , vcl_reply, core);
        httpd_register_url(core, "/vclhelp",  M_GET , vcl_reply, core);
	mk_help(priv);
}

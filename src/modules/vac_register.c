/*
 * Copyright (c) 2012-2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Yves Hwang <yveshwang@gmail.com>
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

#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "http.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>

#include "vsb.h"
struct vac_register_priv_t {
	int logger;
	int curl;
	struct agent_core_t *core;
	struct vsb *vsb_out;
	//vac specific stuff
	char* vac_url;
};

static void generate_json(struct vac_register_priv_t *priv)
{
	assert(priv->vsb_out);
	VSB_clear(priv->vsb_out);
	VSB_printf(priv->vsb_out, "{\n");
	VSB_printf(priv->vsb_out, "\t\"port\": \"%s\",\n", priv->core->config->c_arg);
	VSB_printf(priv->vsb_out, "\t\"user\": ");
	VSB_quote(priv->vsb_out, priv->core->config->user, strlen(priv->core->config->user), 0);
	VSB_printf(priv->vsb_out, ",\n");
	VSB_printf(priv->vsb_out, "\t\"password\": ");
	VSB_quote(priv->vsb_out, priv->core->config->password, strlen(priv->core->config->password), 0);
	VSB_printf(priv->vsb_out, ",\n");
	VSB_printf(priv->vsb_out, "\t\"dash-n\": \"%s\"\n", priv->core->config->n_arg ? priv->core->config->n_arg : "");
	VSB_printf(priv->vsb_out, "}\n");
	assert(VSB_finish(priv->vsb_out) == 0);
}

static int send_curl(struct vac_register_priv_t *private, struct ipc_ret_t *vret)
{
	if (private->vac_url == NULL || *private->vac_url == '\0') {
		logger(private->logger, "Not registering with the VAC. Unknown vac url. Try using -z");
		return -1;
	}
	logger(private->logger, "Registering with the vac: %s", private->vac_url);
	generate_json(private);
	ipc_run(private->curl, vret, "%s\n%s", private->vac_url ? private->vac_url : "", VSB_data(private->vsb_out));
	return 0;
}

static unsigned int vac_register_reply(struct http_request *request, void *data)
{
	//reply callback for the vac_register module to the vac_register module
	struct vac_register_priv_t *vdata = (struct vac_register_priv_t *)data;
	struct ipc_ret_t vret;
	int ret = send_curl(vdata, &vret);
	struct http_response *resp;
	if (ret != 0) {
		send_response_fail(request->connection, "Couldn't register.");
		return 0;
	}
	resp = http_mkresp(request->connection, vret.status, vret.answer);
	logger(vdata->logger, "Curl response: status=%d answer=%s", vret.status, vret.answer);
	send_response2(resp);
	http_free_resp(resp);
	free(vret.answer);
	return 0;
}

static void *vac_register(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct vac_register_priv_t *private;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;
	int ret;
	plug = plugin_find(core,"vac_register");
	assert(plug);
	private = plug->data;
	ret = send_curl(private, &vret);
	if (ret == 0) {
		debuglog(private->logger, "Response received from curl: status=%d answer=%s", vret.status, vret.answer);
		free(vret.answer);
	} else {
		logger(private->logger, "Couldn't register with the VAC");
	}
	return NULL;
}

static pthread_t *vac_register_start(struct agent_core_t *core, const char *name)
{
	(void)name;
	pthread_t *thread = malloc(sizeof (pthread_t));
	pthread_create(thread,NULL,(vac_register),core);
	return thread;
}

void vac_register_init(struct agent_core_t *core) {
	struct vac_register_priv_t *private  =  malloc(sizeof(struct vac_register_priv_t)) ;
	struct agent_plugin_t *plug;
	plug = plugin_find(core, "vac_register");
	assert(plug);

	//initialise the private data structure
	private->logger = ipc_register(core, "logger");
	private->curl = ipc_register(core, "curl");
	private->vsb_out = VSB_new_auto();
	private->core = core;
	/**
	 * XXX: construct the URL based on varnish name, cli setup and vagent's own api location.
         *	pending vac api changes.
	 *
	 *	T_arg or T_arg_orig resides in core-config->T_arg for example. name is n_arg
         */
	private->vac_url = core->config->vac_arg;
	//chuck the private ds to the plugin so it lives on
	plug->data = (void *) private;

	//no need to kick it off just yet
	plug->start = vac_register_start;

	//httpd register
	http_register_url(core, "/vac_register", M_POST, vac_register_reply, private);
}

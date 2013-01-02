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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct status_priv_t {
	int logger;
	int vadmin;
};

static unsigned int run_cmd(struct httpd_request *request, void *data, char *cmd)
{
	struct agent_core_t *core = data;
	struct status_priv_t *status;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;

	plug = plugin_find(core,"status");
	status = plug->data;

	ipc_run(status->vadmin, &vret, cmd);
	send_response_ok(request->connection, vret.answer);
	return 0;
}
unsigned int status_reply(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"status");
	return 0;
}

unsigned int status_stop(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"stop");
	return 0;
}

unsigned int status_start(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"start");
	return 0;
}

void
status_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct status_priv_t *priv = malloc(sizeof(struct status_priv_t));
	plug = plugin_find(core,"status");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/status", M_GET, status_reply, core);
        httpd_register_url(core, "/stop", M_PUT | M_POST, status_stop, core);
        httpd_register_url(core, "/start", M_PUT | M_POST, status_start, core);
}



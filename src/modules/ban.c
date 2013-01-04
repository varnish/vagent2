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

struct ban_priv_t {
	int logger;
	int vadmin;
};

static unsigned int run_cmd(struct httpd_request *request, void *data, const char *cmd)
{
	struct agent_core_t *core = data;
	struct ban_priv_t *ban;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;

	plug = plugin_find(core,"ban");
	ban = plug->data;

	ipc_run(ban->vadmin, &vret, cmd);
	send_response_ok(request->connection, vret.answer);
	return 0;
}

static unsigned int ban_reply(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"ban");
	return 0;
}

static unsigned int ban_stop(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"stop");
	return 0;
}

static unsigned int ban_start(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"start");
	return 0;
}

void
ban_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct ban_priv_t *priv = malloc(sizeof(struct ban_priv_t));
	plug = plugin_find(core,"ban");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
	httpd_register_url(core, "/ban", M_GET, ban_reply, core);
	httpd_register_url(core, "/bstop", M_PUT | M_POST, ban_stop, core);
	httpd_register_url(core, "/bstart", M_PUT | M_POST, ban_start, core);
}

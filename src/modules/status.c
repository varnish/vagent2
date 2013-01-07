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


#define PANIC_HELP \
	"Varnish consists of two processes: The manager and the worker child.\n" \
	"The worker child is the process that does the majority of the work,\n" \
	"while the manager is responsible for \"administrative\" things, like\n" \
	"compiling VCL, talking to the varnish-agent, starting the worker, etc.\n" \
	"\n" \
	"Since the worker is where the majority of the logic lives, this is the one\n" \
	"most vulnerable to bugs. Varnish' typical reaction to bugs is to abort the\n" \
	"worker process and restart it. This typically takes a second or two and can\n" \
	"be hard to notice. It also clears the cache.\n" \
	"\n" \
	"When this occurs, a detailed log is made, which can be retrieved through the\n" \
	"management thread. It is also logged to syslog.\n" \
	"\n" \
	"GET /panic - This retrieves the last panic message\n" \
	"DELETE /panic - This clears the last panic\n" \
	"\n" \
	"Output from GET /panic can be used in bug reports or support tickets.\n" \
	"Keep in mind that not all bugs will be logged here of course. Bugs are tricky.\n"
struct status_priv_t {
	int logger;
	int vadmin;
};

static unsigned int run_cmd(struct httpd_request *request, void *data, const char *cmd)
{
	struct agent_core_t *core = data;
	struct status_priv_t *status;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;

	plug = plugin_find(core,"status");
	status = plug->data;

	ipc_run(status->vadmin, &vret, cmd);
	if (vret.status == 400) {
		send_response_fail(request->connection, vret.answer);
	} else {
		send_response_ok(request->connection, vret.answer);
	}
	free(vret.answer);
	return 0;
}

static unsigned int status_reply(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"status");
	return 0;
}

static unsigned int status_stop(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"stop");
	return 0;
}

static unsigned int status_start(struct httpd_request *request, void *data)
{
	run_cmd(request,data,"start");
	return 0;
}

static unsigned int status_panic(struct httpd_request *request, void *data)
{
	if (request->method == M_GET)
		run_cmd(request,data,"panic.show");
	else if (request->method == M_DELETE)
		run_cmd(request, data, "panic.clear");
	else
		assert("Shouldn't happen");
	return 0;
}


static unsigned int status_panic_help(struct httpd_request *request, void *data)
{
	(void)data;
	send_response_ok(request->connection, PANIC_HELP);
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
	httpd_register_url(core, "/panic", M_GET | M_DELETE, status_panic, core);
	httpd_register_url(core, "/help/panic", M_GET, status_panic_help, core);
}



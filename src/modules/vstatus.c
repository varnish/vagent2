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
#include "http.h"
#include "helpers.h"
#include "config.h"
#include "vcs_version.h"

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

struct vstatus_priv_t {
	int logger;
	int vadmin;
};

static unsigned int vstatus_reply(struct http_request *request, void *data)
{
	struct vstatus_priv_t *vstatus;
	GET_PRIV(data, vstatus);
	run_and_respond(vstatus->vadmin,request->connection,"status");
	return 0;
}

static unsigned int vstatus_stop(struct http_request *request, void *data)
{
	struct vstatus_priv_t *vstatus;
	GET_PRIV(data, vstatus);
	run_and_respond(vstatus->vadmin,request->connection,"stop");
	return 0;
}

static unsigned int vstatus_ping(struct http_request *request, void *data)
{
	struct vstatus_priv_t *vstatus;
	GET_PRIV(data, vstatus);
	run_and_respond(vstatus->vadmin,request->connection,"ping");
	return 0;
}

static unsigned int vstatus_start(struct http_request *request, void *data)
{
	struct vstatus_priv_t *vstatus;
	GET_PRIV(data, vstatus);
	run_and_respond(vstatus->vadmin,request->connection,"start");
	return 0;
}

static unsigned int vstatus_panic(struct http_request *request, void *data)
{
	struct vstatus_priv_t *vstatus;
	GET_PRIV(data, vstatus);
	if (request->method == M_GET)
		run_and_respond_eok(vstatus->vadmin,request->connection,
			200,301,"panic.show");
	else if (request->method == M_DELETE)
		run_and_respond_eok(vstatus->vadmin,request->connection,
			200,301,"panic.clear");
	else
		assert("Shouldn't happen" == NULL);
	return 0;
}

static unsigned int vstatus_version(struct http_request *request, void *data)
{
	(void)data;
	send_response_ok(request->connection, VCS_Version "\n");
	return 0;
}

static unsigned int vstatus_package_string(struct http_request *request, void *data)
{
	(void)data;
	send_response_ok(request->connection, PACKAGE_STRING "\n");
	return 0;
}

void
vstatus_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vstatus_priv_t *priv = malloc(sizeof(struct vstatus_priv_t));
	plug = plugin_find(core,"vstatus");

	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
	http_register_url(core, "/status", M_GET, vstatus_reply, core);
	http_register_url(core, "/stop", M_PUT | M_POST, vstatus_stop, core);
	http_register_url(core, "/start", M_PUT | M_POST, vstatus_start, core);
	http_register_url(core, "/panic", M_GET | M_DELETE, vstatus_panic, core);
	http_register_url(core, "/help/panic", M_GET, help_reply, strdup(PANIC_HELP));
	http_register_url(core, "/version", M_GET, vstatus_version, core);
	http_register_url(core, "/package_string", M_GET, vstatus_package_string, core);
	http_register_url(core, "/ping", M_GET, vstatus_ping, core);
}



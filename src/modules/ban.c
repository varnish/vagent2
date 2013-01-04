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
#include "helpers.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#define BAN_SHORTHAND "req.url ~ "
#define BAN_HELP_TEXT \
	"Banning supports three methods:\n" \
	"GET /ban - list bans (ban.list)\n" \
	"POST /ban - with request body. Uses request body for a literal ban\n" \
	"POST /ban/foo - without request body. Uses the url-part after \"/ban\" to\n" \
	"                ban using " BAN_SHORTHAND " url. E.g: POST /ban/foo: \n" \
	"                ban " BAN_SHORTHAND "/foo\n"

struct ban_priv_t {
	int logger;
	int vadmin;
};

static unsigned int ban_reply(struct httpd_request *request, void *data)
{
	struct ban_priv_t *ban;
	struct agent_plugin_t *plug;
	char *body;
	plug = plugin_find(data,"ban");
	ban = plug->data;

	if (request->method == M_GET) {
		run_and_respond(ban->vadmin, request->connection, "ban.list");
		return 0;
	} else {
		char *mark;
		assert(((char *)request->data)[request->ndata] == '\0');
		body = strdup(request->data);
		mark = index(body,'\n');
		if (mark)
			*mark = '\0';
		if (strlen(request->url) == strlen("/ban"))
			run_and_respond(ban->vadmin, request->connection, "ban %s",body);
		else {
			const char *path = request->url + strlen("/ban");
			if (request->ndata != 0) {
				send_response_fail(request->connection, "Banning with both a url and request body? Pick one or the other please.");
				goto out;
			}
			assert(request->ndata == 0);
			run_and_respond(ban->vadmin, request->connection, "ban " BAN_SHORTHAND "%s",path);
		}
		out:
		free(body);
		return 0;
	}

	return 0;
}

static unsigned int ban_help(struct httpd_request *request, void *data)
{
	struct ban_priv_t *ban;
	struct agent_plugin_t *plug;
	plug = plugin_find(data,"ban");
	ban = plug->data;

	send_response_ok(request->connection, BAN_HELP_TEXT);
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
	httpd_register_url(core, "/ban", M_GET | M_POST, ban_reply, core);
	httpd_register_url(core, "/help/ban", M_GET, ban_help, core);
}

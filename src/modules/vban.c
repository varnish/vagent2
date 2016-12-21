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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "http.h"
#include "helpers.h"
#include "ipc.h"
#include "plugins.h"


/*
 * FIXME: Should be a compile-time option.
 * This is what we use when you do POST /vban/foo
 */
#define BAN_SHORTHAND "req.url ~ "
#define BAN_HELP_TEXT \
	"Banning supports three methods:\n" \
	"GET /ban - list bans (ban.list)\n" \
	"POST /ban - with request body. Uses request body for a literal ban\n" \
	"POST /ban/foo - without request body. Uses the url-part after \"/ban\" to\n" \
	"                ban using " BAN_SHORTHAND " url. E.g: POST /ban/foo: \n" \
	"                ban " BAN_SHORTHAND "/foo\n"

struct vban_priv_t {
	int logger;
	int vadmin;
};

static unsigned int
vban_reply(struct http_request *request, const char *arg, void *data)
{
	struct agent_core_t *core = data;
	struct vban_priv_t *vban;
	char *body;
	char *mark;

	GET_PRIV(core, vban);

	if (request->method == M_GET) {
		run_and_respond(vban->vadmin, request->connection, "ban.list");
		return 0;
	}

	assert(((char *)request->data)[request->ndata] == '\0');
	body = strdup(request->data);
	mark = strchr(body,'\n');
	if (mark)
		*mark = '\0';
	if (!arg)
		run_and_respond(vban->vadmin, request->connection, "ban %s", body);
	else {
		const char *path = request->url + strlen("/ban");
		if (request->ndata != 0) {
			http_reply(request->connection, 500, "Banning with both a url and request body? Pick one or the other please.");
		} else {
			assert(request->ndata == 0);
			run_and_respond(vban->vadmin, request->connection, "ban " BAN_SHORTHAND "/%s",path);
		}
	}
	free(body);

	return 0;
}

void
vban_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vban_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vban");
	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	http_register_url2(core, "/ban", M_GET | M_POST, vban_reply, core);
	http_register_url(core, "/help/ban", M_GET, help_reply, strdup(BAN_HELP_TEXT));
}

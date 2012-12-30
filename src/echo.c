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

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct echo_priv_t {
	int logger;
};

static unsigned int echo_reply(struct httpd_request *request, void *data)
{
	struct echo_priv_t *echo = data;

	logger(echo->logger, "Responding to request");
	send_response(request->connection, 200, request->data, request->ndata);
	return 0;
}

void echo_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct echo_priv_t *priv = malloc(sizeof(struct echo_priv_t));
	plug = plugin_find(core,"echo");
	assert(plug);
	
	priv->logger = ipc_register(core,"logd");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/echo", M_POST | M_PUT | M_GET, echo_reply, priv);
}


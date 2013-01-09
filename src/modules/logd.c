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

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <poll.h>
#include <sys/socket.h>

/*
 * TODO:
 *  - Write to something slightly more elaborate than stdout.
 */
struct logd_priv_t {
	FILE *out;
};

static void read_log(void *private, char *msg, struct ipc_ret_t *ret)
{
	struct logd_priv_t *log = (struct logd_priv_t *) private;
	assert(log);

	fprintf(log->out,"LOGGER: %s\n",msg);
	
	ret->status = 200;
	ret->answer = strdup("OK");
}

void logd_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct logd_priv_t *priv = malloc(sizeof(struct logd_priv_t));
	plug = plugin_find(core,"logd");
	
	priv->out = stdout;
	plug->ipc->cb = read_log;
	plug->ipc->priv = priv;
	plug->data = (void *)priv;
	plug->start = ipc_start;
}

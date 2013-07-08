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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

struct vping_priv_t {
	int vadmin_sock;
	int logger;
};

/*
 * Pings the varnish server every Nth second. First vadmin-plugin written,
 * not sure if it still has value.
 */
static void *vping_run(void *data)
{
	struct agent_core_t *core = (struct agent_core_t *)data;
	struct agent_plugin_t *plug;
	struct vping_priv_t *ping;
	struct ipc_ret_t vret;

	plug = plugin_find(core,"vping");
	ping = (struct vping_priv_t *) plug->data;

	while (1) {
		sleep(30);
		ipc_run(ping->vadmin_sock, &vret, "ping");
		if (vret.status != 200)
			logger(ping->logger, "Ping failed. %d ", vret.status);
		free(vret.answer);

		ipc_run(ping->vadmin_sock, &vret, "status");
		if (vret.status != 200 || strcmp(vret.answer,"Child in state running"))
			logger(ping->logger, "%d %s", vret.status, vret.answer);
		free(vret.answer);
	}
	return NULL;
}

static pthread_t *
vping_start(struct agent_core_t *core, const char *name)
{
	(void)name;
	pthread_t *thread = malloc(sizeof (pthread_t));
	pthread_create(thread,NULL,(*vping_run),core);
	return thread;
}

void
vping_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vping_priv_t *priv = malloc(sizeof(struct vping_priv_t));
	plug = plugin_find(core,"vping");

	priv->vadmin_sock = ipc_register(core,"vadmin");
	priv->logger = ipc_register(core,"logger");
	plug->data = (void *)priv;
	plug->start = vping_start;
}

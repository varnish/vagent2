/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 * Author: Dag-Erling Smørgrav <des@des.no>
 * Author: Kristian Lyngstol <kristian@bohemians.org>
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
 *
 * Statistics output program
 */

#include "config.h"

#define _GNU_SOURCE
#include <sys/time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <varnishapi.h>
#include <time.h>
#include <vsc.h>
#include <pthread.h>
#include "vsb.h"
#include "common.h"
#include "httpd.h"
#include "ipc.h"
#include "plugins.h"
#include "helpers.h"


struct vstat_priv_t {
	struct VSM_data *vd;
	int jp;
	int open;
	const struct VSC_C_main *VSC_C_main;
	struct vsb *vsb_http;
	struct vsb *vsb_timer;
	time_t now;
	int last_uptime;
	pthread_mutex_t lck;
};

static int
do_json_cb(void *priv, const struct VSC_point * const pt)
{
	uint64_t val;
	struct vsb *out_vsb = priv;
	assert(!strcmp(pt->fmt, "uint64_t"));
	val = *(const volatile uint64_t*)pt->ptr;
	assert(sizeof(uint64_t) == sizeof(uintmax_t));

	VSB_printf(out_vsb, ",\n\t\"");
	/* build the JSON key name.  */
	if (pt->class[0])
		VSB_printf(out_vsb,"%s.",pt->class);
	if (pt->ident[0])
		VSB_printf(out_vsb,"%s.",pt->ident);
	VSB_printf(out_vsb,"%s\": {", pt->name);

	if (strcmp(pt->class, ""))
		VSB_printf(out_vsb,"\"type\": \"%s\", ", pt->class);
	if (strcmp(pt->ident, ""))
		VSB_printf(out_vsb,"\"ident\": \"%s\", ", pt->ident);

	VSB_printf(out_vsb, "\"value\": %ju, ", (uintmax_t)val);
	VSB_printf(out_vsb, "\"flag\": \"%c\",", pt->flag);
	VSB_printf(out_vsb, "\"description\": \"%s\" }", pt->desc);
	return (0);
}

static void
do_json(struct vstat_priv_t *vstat, struct vsb *out_vsb)
{
	char time_stamp[20];
	time_t now;
	now = time(NULL);

	assert(out_vsb);
	(void)strftime(time_stamp, 20, "%Y-%m-%dT%H:%M:%S", localtime(&now));
	VSB_printf(out_vsb, "{\n\t\"timestamp\": \"%s\"", time_stamp);
	(void)VSC_Iter(vstat->vd, do_json_cb, out_vsb);
	VSB_printf(out_vsb, "\n}\n");
}

static void vstat_open(struct vstat_priv_t *vstat)
{
	assert(vstat->open == 0);
	vstat->open = VSC_Open(vstat->vd, 1) == 0 ? 1 : 0;
	if (vstat->open) {
		vstat->VSC_C_main = VSC_Main(vstat->vd);
		assert(vstat->VSC_C_main);
	}
}

static int check_reopen(struct vstat_priv_t *vstat)
{
	size_t now = time(NULL);
	if (!vstat->open)
		return 0;
	if (now == vstat->now)
		return 0;
	if (vstat->last_uptime == vstat->VSC_C_main->uptime)
		return 1;
	vstat->last_uptime = vstat->VSC_C_main->uptime;
	vstat->now = now;
	return 0;
}

static unsigned int vstat_reply(struct httpd_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	GET_PRIV(data,vstat);
	pthread_mutex_lock(&vstat->lck);
	if (!vstat->open) {
		vstat_open(vstat);
	}

	if (check_reopen(vstat)) {
		if (vstat->open) {
			VSM_Close(vstat->vd);
			vstat->open = 0;
		}
		vstat_open(vstat);
	}

	if (!vstat->open) {
		pthread_mutex_unlock(&vstat->lck);
		send_response_fail(request->connection, "Couldn't open shmlog");
		return 0;
	}


	do_json(vstat, vstat->vsb_http);
	pthread_mutex_unlock(&vstat->lck);
	assert(VSB_finish(vstat->vsb_http) == 0);
	send_response(request->connection, 200, VSB_data(vstat->vsb_http), VSB_len(vstat->vsb_http));
	VSB_clear(vstat->vsb_http);
	return 0;
}


void
vstat_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vstat_priv_t *priv = malloc(sizeof(struct vstat_priv_t));
	plug = plugin_find(core,"vstat");
	assert(plug);
	
	priv->vd = VSM_New();
	priv->open = 0;
	assert(priv->vd);
	VSC_Setup(priv->vd);
	priv->jp = 0;
	priv->vsb_http = NULL;
	priv->vsb_http = VSB_new_auto();
	plug->data = priv;

	pthread_mutex_init(&priv->lck, NULL);

	if (core->config->n_arg)
		VSC_Arg(priv->vd, 'n', core->config->n_arg);

	httpd_register_url(core, "/stats", M_GET, vstat_reply, core);
	return;
}

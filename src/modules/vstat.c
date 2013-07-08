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
#include "http.h"
#include "ipc.h"
#include "plugins.h"
#include "helpers.h"


struct vstat_priv_t {
	struct VSM_data *vd;
	int jp;
	int open;
	int curl;
	int logger;
	const struct VSC_C_main *VSC_C_main;
	struct vsb *vsb_http;
	struct vsb *vsb_timer;
	time_t now;
	int last_uptime;
	char *push_url;
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

static unsigned int vstat_reply(struct http_request *request, void *data)
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
	struct http_response *resp = http_mkresp(request->connection, 200, NULL);
	resp->data = VSB_data(vstat->vsb_http);
	resp->ndata = VSB_len(vstat->vsb_http);
	http_add_header(resp,"Content-Type","application/json");
	send_response2(resp);
	http_free_resp(resp);
	VSB_clear(vstat->vsb_http);
	return 0;
}

static int push_stats(struct vstat_priv_t *vstat)
{
	struct ipc_ret_t vret;
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
		return -1;
	}

	do_json(vstat, vstat->vsb_timer);
	pthread_mutex_unlock(&vstat->lck);
	assert(VSB_finish(vstat->vsb_timer) == 0);
	if (vstat->push_url)
		ipc_run(vstat->curl, &vret,
			"%s\n%s",vstat->push_url ? vstat->push_url : "http://localhost:8133/",
			VSB_data(vstat->vsb_timer));
	VSB_clear(vstat->vsb_timer);
	return 0;
}

static unsigned int vstat_push_test(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	GET_PRIV(data,vstat);
	if (push_stats(vstat) < 0)
		send_response_fail(request->connection, "Stats pushing failed");
	else
		send_response_ok(request->connection, "Stats pushed");
	return 0;
}



static unsigned int vstat_push_url(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	GET_PRIV(data,vstat);
	pthread_mutex_lock(&vstat->lck);
	assert(request->data);
	if (vstat->push_url)
		free(vstat->push_url);
	vstat->push_url = malloc(request->ndata + 1);
	memcpy(vstat->push_url, request->data, request->ndata);
	vstat->push_url[request->ndata] = '\0';
	logger(vstat->logger, "Got url: \"%s\"", vstat->push_url);
	send_response_ok(request->connection, "Url stored");
	pthread_mutex_unlock(&vstat->lck);
	return 0;
}

static void *vstat_run(void *data)
{
	struct vstat_priv_t *vstat;
	GET_PRIV(data,vstat);
	while (1) {
		sleep(1);
		if (vstat->push_url && *vstat->push_url)
			push_stats(vstat);
	}
	return NULL;
}

static pthread_t *
vstat_start(struct agent_core_t *core, const char *name)
{
	(void)name;
	pthread_t *thread = malloc(sizeof (pthread_t));
	pthread_create(thread,NULL,(*vstat_run),core);
	return thread;
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
	priv->push_url = NULL;
	assert(priv->vd);
	VSC_Setup(priv->vd);
	priv->jp = 0;
	priv->vsb_http = NULL;
	priv->vsb_http = VSB_new_auto();
	priv->vsb_timer = VSB_new_auto();
	plug->data = priv;
	plug->start = vstat_start;
	priv->curl = ipc_register(core, "curl");
	priv->logger = ipc_register(core, "logger");

	pthread_mutex_init(&priv->lck, NULL);

	if (core->config->n_arg)
		VSC_Arg(priv->vd, 'n', core->config->n_arg);

	http_register_url(core, "/stats", M_GET, vstat_reply, core);
	http_register_url(core, "/push/test/stats", M_PUT, vstat_push_test, core);
	http_register_url(core, "/push/url/stats", M_PUT, vstat_push_url, core);
	return;
}

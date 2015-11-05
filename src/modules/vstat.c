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

#define _GNU_SOURCE
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <vapi/vsm.h>
#include <vapi/vsc.h>
#include <pthread.h>

#include "common.h"
#include "http.h"
#include "ipc.h"
#include "plugins.h"
#include "vsb.h"


struct vstat_priv_t {
	struct VSM_data *vd;
	int curl;
	int logger;
	const struct VSC_C_main *VSC_C_main;
	struct vsb *vsb_http;
	struct vsb *vsb_timer;
	char *push_url;
	pthread_mutex_t lck;
};

static int
do_json_cb(void *priv, const struct VSC_point * const pt)
{
	uint64_t val;
	const struct VSC_section *sec;
	struct vsb *out_vsb = priv;

	if (pt == NULL)
		return (0);
	
	assert(!strcmp(pt->desc->ctype, "uint64_t"));
	val = *(const volatile uint64_t*)pt->ptr;
	sec = pt->section;	

	VSB_printf(out_vsb, ",\n\t\"");
	/* build the JSON key name.  */
	if (sec->fantom->type[0])
		VSB_printf(out_vsb,"%s.", sec->fantom->type);
	if (sec->fantom->ident[0])
		VSB_printf(out_vsb,"%s.",sec->fantom->ident);
	VSB_printf(out_vsb,"%s\": {", pt->desc->name);

	if (strcmp(sec->fantom->type, ""))
		VSB_printf(out_vsb,"\"type\": \"%s\", ", sec->fantom->type);
	if (strcmp(sec->fantom->ident, ""))
		VSB_printf(out_vsb,"\"ident\": \"%s\", ", sec->fantom->ident);

	VSB_printf(out_vsb, "\"value\": %" PRIu64 ", ", val);
	VSB_printf(out_vsb, "\"flag\": \"%c\", ", pt->desc->semantics);
	VSB_printf(out_vsb, "\"description\": \"%s\" }", pt->desc->sdesc);
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
	(void)VSC_Iter(vstat->vd, NULL, do_json_cb, out_vsb);
	VSB_printf(out_vsb, "\n}\n");
}

static int check_reopen(struct vstat_priv_t *vstat)
{
	if (VSM_Abandoned(vstat->vd)) {
		VSM_Close(vstat->vd);
		return (VSM_Open(vstat->vd) != 0);
	}

	return (0);
}

static unsigned int vstat_reply(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	pthread_mutex_lock(&vstat->lck);

	if (check_reopen(vstat)) {
		pthread_mutex_unlock(&vstat->lck);
		http_reply(request->connection, 500, "Couldn't open shmlog");
		return 0;
	}

	do_json(vstat, vstat->vsb_http);
	pthread_mutex_unlock(&vstat->lck);
	assert(VSB_finish(vstat->vsb_http) == 0);
	struct http_response *resp = http_mkresp(request->connection, 200, NULL);
	resp->data = VSB_data(vstat->vsb_http);
	resp->ndata = VSB_len(vstat->vsb_http);
	http_add_header(resp,"Content-Type","application/json");
	send_response(resp);
	http_free_resp(resp);
	VSB_clear(vstat->vsb_http);
	return 0;
}

/*
 * Push stats to url.
 *
 * Must have vstat->lck, as this is called from at least two threads and
 * access shared resoruces (vstat->push_url and vstat->vsb_timer). 
 *
 * The lock ensures that a test will have to wait for the timer to complete
 * and vice versa.
 */
static int push_stats(struct vstat_priv_t *vstat)
{
	struct ipc_ret_t vret;

	if (!vstat->push_url) {
		logger(vstat->logger,"Tried to push without a URL");
		return -1;
	}
	if (check_reopen(vstat)) {
		return -1;
	}

	do_json(vstat, vstat->vsb_timer);
	assert(VSB_finish(vstat->vsb_timer) == 0);
	ipc_run(vstat->curl, &vret, "%s\n%s",vstat->push_url, VSB_data(vstat->vsb_timer));
	if (vret.status != 200)
		logger(vstat->logger,"cURL returned %d: %s", vret.status, vret.answer);
	VSB_clear(vstat->vsb_timer);
	free(vret.answer);
	return 0;
}

static unsigned int vstat_push_test(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	logger(vstat->logger,"Test issued, trying to push stats");
	pthread_mutex_lock(&vstat->lck);
	if (push_stats(vstat) < 0)
		http_reply(request->connection, 500, "Stats pushing failed");
	else
		http_reply(request->connection, 200, "Stats pushed");
	pthread_mutex_unlock(&vstat->lck);
	return 0;
}



static unsigned int
vstat_push_url(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	pthread_mutex_lock(&vstat->lck);
	if (vstat->push_url)
		free(vstat->push_url);
	DUP_OBJ(vstat->push_url, request->data, request->ndata);
	logger(vstat->logger, "Got url: \"%s\"", vstat->push_url);
	pthread_mutex_unlock(&vstat->lck);
	http_reply(request->connection, 200, "Url stored");
	return (0);
}

static void *vstat_run(void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	while (1) {
		sleep(1);
		pthread_mutex_lock(&vstat->lck);
		if (vstat->push_url && *vstat->push_url)
			push_stats(vstat);
		pthread_mutex_unlock(&vstat->lck);
	}
	return NULL;
}

static void *
vstat_start(struct agent_core_t *core, const char *name)
{
	pthread_t *thread;

	(void)name;

	ALLOC_OBJ(thread);
	AZ(pthread_create(thread, NULL, (*vstat_run), core));
	return (thread);
}

void
vstat_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vstat_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vstat");

	priv->vd = VSM_New();
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
}

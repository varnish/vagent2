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
int cont = 0;
uint64_t  beresp_hdr = 0, beresp_body = 0;
uint64_t  bereq_hdr = 0, bereq_body = 0;
int n_be = 1;
int creepy_math(void *priv, const struct VSC_point *const pt);

/*
 * There are two threads: The http-replier and the background timer for
 * pushing. Duplicating curl- and logger- fd's is a no-brainer: That's what
 * it's for. Duplicating *vd and *vsb considerably simplifies things, and
 * will only cause a slight memory-increase.
 */
struct vstat_thread_ctx_t {
	struct VSM_data *vd;
	struct vsb *vsb;
	int curl;
	int logger;
};

/*
 * push_url is the only thing requiring a lock.
 */
struct vstat_priv_t {
	struct vstat_thread_ctx_t http;
	struct vstat_thread_ctx_t timer;
	char *push_url;
	pthread_rwlock_t lck;
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

// int
// creepy_math(void *priv, const struct VSC_point *const pt)
// {
//         const struct VSC_section *sec;
//         uint64_t val;
//
//         struct vsb *out_vsb = priv;
//
//         if (pt == NULL)
//                 return (0);
//
//         val = *(const volatile uint64_t *)pt->ptr;
//         sec = pt->section;
//         if(!strcmp(sec->fantom->type,"MAIN")){
//                 if(!strcmp(pt->desc->name, "n_backend")){
//                     n_be = (int)val;
//                 }
//         }
//         if (!strcmp(sec->fantom->type, "VBE")) {
//                 if(!strcmp(pt->desc->name, "bereq_hdrbytes"))
//                     bereq_hdr = val;
//                 if(!strcmp(pt->desc->name, "bereq_bodybytes")) {
//                     bereq_body = val;
//                     VSB_cat(out_vsb, "{\"ident\":\"");
//                     VSB_cat(out_vsb, pt->section->fantom->ident);
//                     VSB_printf(out_vsb,"\", \"bereq_tot\": %" PRIu64 ",",
//                         bereq_body + bereq_hdr);
//                 }
//
//                 if(!strcmp(pt->desc->name, "beresp_hdrbytes"))
//                     beresp_hdr = val;
//                 if(!strcmp(pt->desc->name, "beresp_bodybytes")) {
//                     beresp_body = val;
//                     VSB_printf(out_vsb,"\"beresp_tot\": %" PRIu64 "}",
//                         beresp_body + beresp_hdr);
// 						if(cont < (n_be - 1)) {
// 								VSB_cat(out_vsb, ",\n\t\t");
// 							  cont++;
// 						}
//         }
//         }
//         return(0);
// }


static void
do_json(struct VSM_data *vd, struct vsb *out_vsb)
{
	char time_stamp[20];
	time_t now;
	now = time(NULL);

	assert(out_vsb);
	(void)strftime(time_stamp, 20, "%Y-%m-%dT%H:%M:%S", localtime(&now));
	VSB_printf(out_vsb, "{\n\t\"timestamp\": \"%s\"", time_stamp);
	(void)VSC_Iter(vd, NULL, do_json_cb, out_vsb);

 // 	VSB_cat(out_vsb, ",\n\t\"be_bytes\": [");
    //     // (void)VSC_Iter(vd, NULL, creepy_math, out_vsb);
    //     VSB_cat(out_vsb, "]\n");
    //     cont= 0;
    // 	// remove last comma
	// 	// if(strcmp(VSB_data(out_vsb[out_vsb->s_len -4]) , "," )){
	// 	// 	VSB_setpos(out_vsb, out_vsb->s_len - 4);
	// 	// }
	VSB_printf(out_vsb, "\n}\n");
	assert(VSB_finish(out_vsb) == 0);
}

static int
check_reopen(struct vstat_thread_ctx_t *ctx)
{
	int ret = 0;
	if (VSM_Abandoned(ctx->vd)) {
		VSM_Close(ctx->vd);
		ret = VSM_Open(ctx->vd) != 0;
	}
	if (ret) {
		logger(ctx->logger, "Failed to open the shmlog");
	}
	return ret;
}

static unsigned int
vstat_reply(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;
	struct http_response *resp;

	GET_PRIV(core, vstat);

	if (check_reopen(&vstat->http)) {
		http_reply(request->connection, 500, "Couldn't open shmlog");
		return 0;
	}

	do_json(vstat->http.vd, vstat->http.vsb);

	resp = http_mkresp(request->connection, 200, NULL);
	resp->data = VSB_data(vstat->http.vsb);
	resp->ndata = VSB_len(vstat->http.vsb);
	http_add_header(resp,"Content-Type","application/json");
	send_response(resp);
	http_free_resp(resp);
	VSB_clear(vstat->http.vsb);
	return 0;
}

/*
 * Push stats to url.
 *
 * Called from different threads due to /push/test/stats
 */
static int
push_stats(struct vstat_priv_t *vstat, struct vstat_thread_ctx_t *ctx)
{
	struct ipc_ret_t vret;
	int ret = 0;
	pthread_rwlock_rdlock(&vstat->lck);
	if (!vstat->push_url || !(*vstat->push_url) || check_reopen(ctx)) {
		pthread_rwlock_unlock(&vstat->lck);
		return -1;
	}

	do_json(ctx->vd, ctx->vsb);
	ipc_run(ctx->curl, &vret, "%s\n%s",vstat->push_url, VSB_data(ctx->vsb));
	pthread_rwlock_unlock(&vstat->lck);
	VSB_clear(ctx->vsb);
	if (vret.status != 200) {
		logger(ctx->logger,"cURL returned %d: %s", vret.status, vret.answer);
		ret = -1;
	}
	assert(vret.answer);
	free(vret.answer);
	return ret;
}

static unsigned int
vstat_push_test(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	if (push_stats(vstat, &vstat->http) < 0)
		http_reply(request->connection, 500, "Stats pushing failed");
	else
		http_reply(request->connection, 200, "Stats pushed");
	return 0;
}

static unsigned int
vstat_push_url(struct http_request *request, void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	pthread_rwlock_wrlock(&vstat->lck);
	if (vstat->push_url)
		free(vstat->push_url);
	DUP_OBJ(vstat->push_url, request->data, request->ndata);
	logger(vstat->http.logger, "Got url: \"%s\"", vstat->push_url);
	pthread_rwlock_unlock(&vstat->lck);
	http_reply(request->connection, 200, "Url stored");
	return (0);
}

static void *
vstat_run(void *data)
{
	struct vstat_priv_t *vstat;
	struct agent_core_t *core = data;

	GET_PRIV(core, vstat);
	while (1) {
		sleep(1);
		/*
		 * FIXME: This whole thing is bonkers.
		 */
		if (push_stats(vstat, &vstat->timer) < 0)
			sleep(10);
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

static void
vstat_init_ctx(struct agent_core_t *core, struct vstat_thread_ctx_t *t_ctx)
{
	t_ctx->vd = VSM_New();
	t_ctx->vsb = VSB_new_auto();
	t_ctx->curl = ipc_register(core,"curl");
	t_ctx->logger = ipc_register(core,"logger");
	if (core->config->n_arg)
		VSC_Arg(t_ctx->vd, 'n', core->config->n_arg);
}

void
vstat_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vstat_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vstat");
	vstat_init_ctx(core,&priv->http);
	vstat_init_ctx(core,&priv->timer);

	plug->data = priv;
	plug->start = vstat_start;

	pthread_rwlock_init(&priv->lck, NULL);

	http_register_url(core, "/stats", M_GET, vstat_reply, core);
	http_register_url(core, "/push/test/stats", M_PUT, vstat_push_test, core);
	http_register_url(core, "/push/url/stats", M_PUT, vstat_push_url, core);
}

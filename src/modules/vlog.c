/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
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
 * varnishlog-module for varnish-agent.
 * Original varnishlog by phk, varnish-agent work by Kristian.
 * All bugs introduced by unknown third party.
 */

/*
 * This plugin still has its flaws ..
 *
 * We plan on doing a proper vlog module sometime down the line, where we
 * properly leverage the flexibility of the new logging API in Varnish 4.0.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "http.h"
#include "ipc.h"
#include "plugins.h"
#include "vsb.h"

#include <vapi/vsm.h>
#include <vapi/vsl.h>


/* Borrow these from vsl_dispatch.c */
static const char * const vsl_t_names[VSL_t__MAX] = {
	[VSL_t_unknown]	= "unknown",
	[VSL_t_sess]	= "sess",
	[VSL_t_req]	= "req",
	[VSL_t_bereq]	= "bereq",
	[VSL_t_raw]	= "raw",
};

static const char * const vsl_r_names[VSL_r__MAX] = {
	[VSL_r_unknown]	= "unknown",
	[VSL_r_http_1]	= "HTTP/1",
	[VSL_r_rxreq]	= "rxreq",
	[VSL_r_esi]	= "esi",
	[VSL_r_restart]	= "restart",
	[VSL_r_pass]	= "pass",
	[VSL_r_fetch]	= "fetch",
	[VSL_r_bgfetch]	= "bgfetch",
};

struct vlog_priv_t {
	int logger;
};

struct vlog_req_priv {
	struct vsb *answer;
	struct VSM_data *vsm;
	unsigned limit;
	unsigned entries;
};

static int
vlog_cb_func(struct VSL_data *vsl, struct VSL_transaction * const trans[],
    void *priv)
{
	int i;
	struct vlog_req_priv *vrp = priv;
	struct VSL_transaction *t;

	for (i = 0; (t = trans[i]) != NULL && vrp->entries < vrp->limit; ++i) {
		while (VSL_Next(t->c) && vrp->entries < vrp->limit) {
			if (!VSL_Match(vsl, t->c)) {
				continue;
			}

			if (vrp->entries != 0)
				VSB_printf(vrp->answer,",");
			VSB_printf(vrp->answer,"\n");
			VSB_printf(vrp->answer, "{ \"vxid\": \"%u\", "
			    "\"tag\": \"%s\", \"type\": \"%s\", \"reason\": \"%s\", "
			    "\"value\": ",
			    t->vxid, VSL_tags[VSL_TAG(t->c->rec.ptr)],
			    vsl_t_names[t->type], vsl_r_names[t->reason]);
			VSB_quote(vrp->answer, VSL_CDATA(t->c->rec.ptr),
			    strlen(VSL_CDATA(t->c->rec.ptr)), 0);
			VSB_printf(vrp->answer,"}");
			vrp->entries++;
		}
	}
	
	return (0);
}

static char *
next_slash(const char *p)
{
	char *ret;
	ret = strchr(p, '/');
	if (ret != NULL)
		ret++;
	if (ret && *ret == '\0')
		ret = NULL;
	return ret;
}

static unsigned int
vlog_reply(struct http_request *request, void *data)
{
	struct vlog_req_priv vrp = { .limit = 10 };
	int disp_status;
	char *p;
	char *tag = NULL;
	char *tag_re = NULL;
	struct VSL_data *vsl = NULL;
	struct VSLQ *vslq = NULL;
	struct VSL_cursor *c = NULL;
	enum VSL_grouping_e grouping = VSL_g_request;
	struct agent_core_t *core = data;

	p = next_slash(request->url + 1);
	if (p) {
		char *lim = strdup(p);
		assert(lim);
		char *tmp2 = strchr(lim, '/');
		if (tmp2 && *tmp2) *tmp2 = '\0';

		int j = sscanf(lim, "%u", &vrp.limit);
		if(j != 1) {
			free(lim);
			http_reply(request->connection, 500, "Not a number");
			return 0;
		}

		free(lim);
		p = next_slash(p);
	}

	if (p) {
		tag = strdup(p);
		char *tmp2 = strchr(tag,'/');
		if (tmp2 && *tmp2) *tmp2 = '\0';
		p = next_slash(p);
	}

	if (p) {
		tag_re = strdup(p);
		char *tmp2 = strchr(tag_re, '/');
		if (tmp2 && *tmp2) *tmp2 = '\0';
		p = next_slash(p);
	}
	
	vrp.answer = VSB_new_auto();
	assert(vrp.answer != NULL);

	vrp.vsm = VSM_New();
	assert(vrp.vsm);
	if (!VSM_n_Arg(vrp.vsm, core->config->n_arg)) {
		VSB_printf(vrp.answer, "Error in creating shmlog: %s",
		    VSM_Error(vrp.vsm));
		VSB_finish(vrp.answer);
		http_reply(request->connection, 500, VSB_data(vrp.answer));
		goto cleanup;
	}

	if (VSM_Open(vrp.vsm) != 0) {
		VSB_printf(vrp.answer, "Error in opening shmlog: %s",
		    VSM_Error(vrp.vsm));
		VSB_finish(vrp.answer);
		http_reply(request->connection, 500, VSB_data(vrp.answer));
		goto cleanup;
	}
	
	vsl = VSL_New();
	assert(vsl);

	if (tag) {
		grouping = VSL_g_raw;
		
		if (VSL_Arg(vsl, 'i', tag) < 0) {
			VSB_printf(vrp.answer, "Unable to specify tag '%s': %s",
			    tag, VSL_Error(vsl));
			VSB_finish(vrp.answer);
			http_reply(request->connection, 500, VSB_data(vrp.answer));
			goto cleanup;
		}
		if (tag_re) {
			VSL_Arg(vsl,'I', tag_re);
		}
	}

	c = VSL_CursorVSM(vsl, vrp.vsm,
	    VSL_COPT_BATCH | VSL_COPT_TAILSTOP);
	if (c == NULL) {
		VSB_printf(vrp.answer, "Can't open log (%s)",
		    VSL_Error(vsl));
		VSB_finish(vrp.answer);
		http_reply(request->connection, 500, VSB_data(vrp.answer));
		goto cleanup;
	}

	
	vslq = VSLQ_New(vsl, &c, grouping, NULL);
	if (vslq == NULL) {
		VSB_clear(vrp.answer);
		VSB_printf(vrp.answer, "Error in creating query: %s",
		    VSL_Error(vsl));
		http_reply(request->connection, 500, VSB_data(vrp.answer));
		goto cleanup;
	}

	VSB_printf(vrp.answer, "{ \"log\": [");

	do {
		disp_status = VSLQ_Dispatch(vslq, vlog_cb_func, &vrp);
	} while (disp_status == 1 && vrp.entries < vrp.limit);

	VSB_printf(vrp.answer, "\n] }\n");

	assert(VSB_finish(vrp.answer) == 0);
	if (VSB_len(vrp.answer) > 1) {
		struct http_response *resp = http_mkresp(request->connection, 200, NULL);
		resp->data = VSB_data(vrp.answer);
		resp->ndata = VSB_len(vrp.answer);
		http_add_header(resp,"Content-Type","application/json");
		send_response(resp);
		http_free_resp(resp);
	} else {
		http_reply(request->connection, 500, "FAIL");
	}

 cleanup:
	free(tag);
	free(tag_re);
	VSB_delete(vrp.answer);
	if (vslq)
		VSLQ_Delete(&vslq);
	if (vsl)
		VSL_Delete(vsl);
	if (vrp.vsm)
		VSM_Delete(vrp.vsm);
	vrp.answer = NULL;
	return 0;
}

void
vlog_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vlog_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vlog");
	plug->data = priv;

	http_register_url(core, "/log", M_GET, vlog_reply, core);
}

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
 * XXX: Huge XXX....
 *
 * The entire varnishlod -d logic is flawed, and I've done little to
 * mitigate it.
 *
 * First: It will read from the beginning of the file (shmlog), and to
 * where-ever varnish is writing to. If varnish just wrapped around, you
 * get almost no data.
 *
 * Second: The -k logic works as |head, not tail, when combined with -d.
 *
 * Third: -k and -d in varnishlog is broken completely:
 * kristian@freud:~$ varnishlog -d -k 100   | wc -l
 * 10554
 *
 * Fourth: As is -k, -d and -i:
 * kristian@freud:~$ varnishlog -d -k 100 -i ReqStart   | wc -l
 * 86
 * kristian@freud:~$ varnishlog -d -k 10 -i ReqStart   | wc -l
 * 85
 *
 * The correct fix for these issues is to fix the API, which Martin is
 * doing for 3.0+1 (4.0? 3.1? 3.14?). The fix may or may not fix -k for -d.
 *
 * As such, this plugin is of limited use. It's made available in it's
 * neutered form to allow frontend testing awaiting the next Varnish
 * release.
 */
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <varnishapi.h>
#include "common.h"
#include "http.h"
#include "ipc.h"
#include "plugins.h"
#include "vsb.h"
#include "helpers.h"

struct vlog_priv_t {
	int logger;
	struct VSM_data *vd;
	struct vsb	*ob[65536];
	unsigned char	flg[65536];
	enum VSL_tag_e   last[65536];
	uint64_t       bitmap[65536];
	struct vsb	*answer;
	int entries;
	char *tag;
};

/* Ordering-----------------------------------------------------------*/

/*
 * Print a single line of varnishlog as json, possibly adding a comma.
 *
 * XXX: This concept is slightly flawed. It is global, which means that the
 * comma-logic can mess up. Assume that the first line /read/ is part of a
 * session, but that the first line output is not. If that happens, the
 * first entry output will have a comma prefixed. Plain wrong.
 */
static void print_entry(struct vlog_priv_t *vlog, int fd,
			enum VSL_tag_e tag, int type,
			const char *ptr, unsigned len)
{
	if (vlog->entries > 0)
		VSB_printf(vlog->answer,",");
	VSB_printf(vlog->answer,"\n");
	VSB_printf(vlog->answer, "{ \"fd\": \"%d\","
		   "\"tag\": \"%s\", \"type\":\"%c\","
		   "\"value\":",
		   fd, VSL_tags[tag], type);
	VSB_quote(vlog->answer, ptr, len, 0);
	VSB_printf(vlog->answer,"}");
	vlog->entries++;
}

static int local_print(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
    unsigned spec, const char *ptr, uint64_t bitmap2)
{
	struct vlog_priv_t *vlog = priv;
	int type;

	(void) bitmap2;

	type = (spec & VSL_S_CLIENT) ? 'c' :
		(spec & VSL_S_BACKEND) ? 'b' : '-';

	if (tag == SLT_Debug) {
		if (vlog->entries > 0)
			VSB_printf(vlog->answer,",");
		VSB_printf(vlog->answer, "\n{ \"fd\": \"%d\","
			   "\"tag\": \"%s\", \"type\":\"%c\","
			   "\"value\":\"",
			   fd, VSL_tags[tag], type);
		while (len-- > 0) {
			if (*ptr >= ' ' && *ptr <= '~')
				VSB_printf(vlog->answer, "%c", *ptr);
			else
				VSB_printf(vlog->answer, "%%%02x", (unsigned char)*ptr);
			ptr++;
		}
		VSB_printf(vlog->answer,"\"}");
		return (0);
	}
	print_entry(vlog,fd,tag, type, ptr, len);
	return (0);
}

static void
h_order_finish(struct vlog_priv_t *vlog, int fd)
{
	assert(VSB_finish(vlog->ob[fd]) == 0);
	if (VSB_len(vlog->ob[fd]) > 1 && VSL_Matched(vlog->vd, vlog->bitmap[fd])) {
		VSB_printf(vlog->answer,"%s", VSB_data(vlog->ob[fd]));
	}
	vlog->bitmap[fd] = 0;
	VSB_clear(vlog->ob[fd]);
}

static void
clean_order(struct vlog_priv_t *vlog)
{
	unsigned u;

	for (u = 0; u < 65536; u++) {
		if (vlog->ob[u] == NULL)
			continue;
		assert(VSB_finish(vlog->ob[u]) == 0);
		if (VSB_len(vlog->ob[u]) > 1 && VSL_Matched(vlog->vd, vlog->bitmap[u])) {
			VSB_printf(vlog->answer,"%s\n", VSB_data(vlog->ob[u]));
		}
		vlog->flg[u] = 0;
		vlog->bitmap[u] = 0;
		VSB_clear(vlog->ob[u]);
	}
}

static int
h_unorder(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
    unsigned spec, const char *ptr, uint64_t bm)
{
	char type;
	struct vlog_priv_t *vlog = priv;
	type = (spec & VSL_S_CLIENT) ? 'c' :
	    (spec & VSL_S_BACKEND) ? 'b' : '-';
	local_print(vlog,tag,fd,len,type,ptr,bm);
	return 0;
}

static int
h_order(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
    unsigned spec, const char *ptr, uint64_t bm)
{
	char type;
	struct vlog_priv_t *vlog = priv;

	/* XXX: Just ignore any fd not inside the bitmap */
	if (fd >= sizeof vlog->bitmap / sizeof vlog->bitmap[0])
		return (0);

	vlog->bitmap[fd] |= bm;

	type = (spec & VSL_S_CLIENT) ? 'c' :
	    (spec & VSL_S_BACKEND) ? 'b' : '-';

	if (!(spec & (VSL_S_CLIENT|VSL_S_BACKEND))) {
			(void)local_print(vlog, tag, fd, len, spec, ptr, bm);
		return (0);
	}
	if (vlog->ob[fd] == NULL) {
		vlog->ob[fd] = VSB_new_auto();
		assert(vlog->ob[fd] != NULL);
	}
	if ((tag == SLT_BackendOpen || tag == SLT_SessionOpen ||
		(tag == SLT_ReqStart &&
		    vlog->last[fd] != SLT_SessionOpen &&
		    vlog->last[fd] != SLT_VCL_acl) ||
		(tag == SLT_BackendXID &&
		    vlog->last[fd] != SLT_BackendOpen)) &&
	    VSB_len(vlog->ob[fd]) != 0) {
		/*
		 * This is the start of a new request, yet we haven't seen
		 * the end of the previous one.  Spit it out anyway before
		 * starting on the new one.
		 */
		if (vlog->last[fd] != SLT_SessionClose)
			VSB_printf(vlog->ob[fd], "{ \"fd\": \"%d\","
				"\"tag\": \"%s\", \"type\":\"%c\","
				"\"value\":\"%s\"},\n",
			    fd, "Interrupted", type, VSL_tags[tag]);
		h_order_finish(vlog, fd);
	}

	vlog->last[fd] = tag;

	print_entry(vlog, fd, tag, type, ptr, len);
	switch (tag) {
	case SLT_ReqEnd:
	case SLT_BackendClose:
	case SLT_BackendReuse:
	case SLT_StatSess:
		h_order_finish(vlog, fd);
		break;
	default:
		break;
	}
	return (0);
}

static void
do_order(struct vlog_priv_t *vlog)
{
	int i;
		VSL_Select(vlog->vd, SLT_SessionOpen);
		VSL_Select(vlog->vd, SLT_SessionClose);
		VSL_Select(vlog->vd, SLT_ReqEnd);
		VSL_Select(vlog->vd, SLT_BackendOpen);
		VSL_Select(vlog->vd, SLT_BackendClose);
		VSL_Select(vlog->vd, SLT_BackendReuse);
	while (1) {
		i = VSL_Dispatch(vlog->vd, h_order, vlog);
		if (i == 0) {
			clean_order(vlog);
		}
		else if (i < 0)
			break;
	}
	clean_order(vlog);
}
static void do_unorder(struct vlog_priv_t *vlog)
{
	int i;
	while (1) {
		i = VSL_Dispatch(vlog->vd, h_unorder, vlog);
		if ( i < 0)
			break;
	}
}

static char *next_slash(const char *p)
{
	char *ret;
	ret = index(p, '/');
	if (ret != NULL)
		ret++;
	if (ret && *ret == '\0')
		ret = NULL;
	return ret;
}

static unsigned int vlog_reply(struct http_request *request, void *data)
{
	struct vlog_priv_t *vlog;
	int ret;
	char *limit = NULL;
	char *p;
	char *tag = NULL;
	char *itag = NULL;
	struct agent_core_t *core = data;
	GET_PRIV(data,vlog);
	p = next_slash(request->url + 1);

	assert(vlog->tag==NULL);
	assert(vlog->answer == NULL);

	if (p) {
		limit = strdup(p);
		assert(limit);
		char *tmp2 = index(limit,'/');
		if (tmp2 && *tmp2) *tmp2 = '\0';

		if(!(atoi(limit) > 0)) {
			free(limit);
			send_response_fail(request->connection,"Not a number");
			return 0;
		}
		p = next_slash(p);
	}
	if (p) {
		tag = strdup(p);
		char *tmp2 = index(tag,'/');
		if (tmp2 && *tmp2) *tmp2 = '\0';
		p = next_slash(p);
	}
	if (p) {
		itag = strdup(p);
		char *tmp2 = index(itag,'/');
		if (tmp2 && *tmp2) *tmp2 = '\0';
		p = next_slash(p);
	}
	vlog->answer = VSB_new_auto();
	assert(vlog->answer != NULL);
	vlog->vd = VSM_New();
	assert(VSL_Arg(vlog->vd, 'n', core->config->n_arg));
	VSL_Setup(vlog->vd);
	VSL_Arg(vlog->vd, 'd', "");
	if (tag) {
		VSL_Arg(vlog->vd, 'i', tag);
		if (itag)
			VSL_Arg(vlog->vd,'I',itag);
	} else {
		VSL_Arg(vlog->vd, 'k', limit ? limit : "10");
	}

	if (limit)
		free(limit);
	VSB_printf(vlog->answer, "{ \"log\": [");
	ret = VSL_Open(vlog->vd, 1);
	assert(!ret);

	if (tag == NULL) {
		do_order(vlog);
	} else {
		do_unorder(vlog);
	}
	if (tag)
		free(tag);
	if (itag)
		free(itag);

	VSB_printf(vlog->answer, "\n] }\n");
	assert(VSB_finish(vlog->answer) == 0);
	if (VSB_len(vlog->answer) > 1) {
		struct http_response *resp = http_mkresp(request->connection, 200, NULL);
		resp->data = VSB_data(vlog->answer);
		resp->ndata = VSB_len(vlog->answer);
		http_add_header(resp,"Content-Type","application/json");
		send_response2(resp);
		http_free_resp(resp);
	} else {
		send_response_fail(request->connection, "FAIL");
	}
	VSB_clear(vlog->answer);
	VSB_delete(vlog->answer);
	VSM_Delete(vlog->vd);
	vlog->answer = NULL;
	vlog->entries = 0;
	return 0;
}

void vlog_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vlog_priv_t *priv = malloc(sizeof(struct vlog_priv_t));
	plug = plugin_find(core,"vlog");
	assert(plug);
	plug->data = priv;

	http_register_url(core, "/log", M_GET, vlog_reply, core);
}

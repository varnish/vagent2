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

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <varnishapi.h>
#include <assert.h>
#include "common.h"
#include "httpd.h"
#include "ipc.h"
#include "plugins.h"
#include "vsb.h"

struct vlog_priv_t {
	int logger;
	struct VSM_data *vd;
	struct vsb	*ob[65536];
	unsigned char	flg[65536];
	enum VSL_tag_e   last[65536];
	uint64_t       bitmap[65536];
	struct vsb	*answer;
	int entries;
};

/* Ordering-----------------------------------------------------------*/


static void print_escaped(struct vsb *target, const char *string, int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (string[i] == '\"' || string[i] == '\\') {
			VSB_printf(target, "\\");
		}
		VSB_printf(target,"%c",string[i]);
	}
}

static void print_entry(struct vlog_priv_t *vlog, int fd,
			enum VSL_tag_e tag, int type,
			const char *ptr, unsigned len)
{
	if (vlog->entries > 0)
		VSB_printf(vlog->answer,",");
	VSB_printf(vlog->answer,"\n");
	VSB_printf(vlog->answer, "{ \"fd\": \"%d\","
		   "\"tag\": \"%s\", \"type\":\"%c\","
		   "\"value\":\"",
		   fd, VSL_tags[tag], type);
	print_escaped(vlog->answer,ptr, len);
	VSB_printf(vlog->answer,"\"}");
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

/*--------------------------------------------------------------------*/

static unsigned int vlog_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct agent_plugin_t *plug;
	struct vlog_priv_t *vlog;
	int ret;

	plug = plugin_find(core,"vlog");
	assert(plug);
	vlog = plug->data;

	assert(vlog->answer == NULL);
	vlog->answer = VSB_new_auto();
	assert(vlog->answer != NULL);
	
	vlog->vd = VSM_New();
	VSL_Setup(vlog->vd);
	VSL_Arg(vlog->vd, 'd', "");
	VSL_Arg(vlog->vd, 'k',"100");
	VSB_printf(vlog->answer, "{ \"log\": [\n");
	ret = VSL_Open(vlog->vd, 1);
	assert(!ret);

	do_order(vlog);

	VSB_printf(vlog->answer, "\n] }\n");
	assert(VSB_finish(vlog->answer) == 0);
	if (VSB_len(vlog->answer) > 1) {
		send_response(request->connection, 200, VSB_data(vlog->answer), VSB_len(vlog->answer));
	} else {
		send_response_fail(request->connection, "FAIL");
	}
	VSB_clear(vlog->answer);
	VSM_Close(vlog->vd);
	vlog->answer = NULL;
	return 0;
}

void vlog_init(struct agent_core_t *core)
{

	struct agent_plugin_t *plug;
	struct vlog_priv_t *priv = malloc(sizeof(struct vlog_priv_t));
	plug = plugin_find(core,"vlog");
	assert(plug);
	plug->data = priv;

	httpd_register_url(core, "/log", M_GET, vlog_reply, core);
}

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
#include <assert.h>
#include "common.h"
#include "httpd.h"
#include "ipc.h"
#include "plugins.h"
#include "helpers.h"


struct varnishstat_priv_t {
	struct VSM_data *vd;
	int jp;
	char *buffer;
	const struct VSC_C_main *VSC_C_main;
};

/*
 * FIXME: Should use VSB_printf!
 */
static int
do_json_cb(void *priv, const struct VSC_point * const pt)
{
	uint64_t val;
	struct varnishstat_priv_t *varnishstat = priv;
	char buffer[4096+1];
	char buffer2[4096+1];
	buffer2[0] = buffer[0] = '\0';
	assert(!strcmp(pt->fmt, "uint64_t"));
	val = *(const volatile uint64_t*)pt->ptr;
	assert(sizeof(uint64_t) == sizeof(uintmax_t));

	if (varnishstat->jp)
		varnishstat->jp = 0;
	else {
		snprintf(buffer,4096,",\n");
		strncat(buffer2,buffer,4096);
	}
	snprintf(buffer,4096,"\t\"");
	strncat(buffer2,buffer,4096);
	/* build the JSON key name.  */
	if (pt->class[0]) {
		snprintf(buffer,4096,"%s.", pt->class);
		strncat(buffer2,buffer,4096);
	}
	if (pt->ident[0]) {
		snprintf(buffer,4096,"%s.", pt->ident);
		strncat(buffer2,buffer,4096);
	}
	snprintf(buffer,4096,"%s\": {", pt->name);
	strncat(buffer2,buffer,4096);

	if (strcmp(pt->class, "")) {
		snprintf(buffer,4096,"\"type\": \"%s\", ",  pt->class);
		strncat(buffer2,buffer,4096);
	}
	if (strcmp(pt->ident, "")) {
		snprintf(buffer,4096,"\"ident\": \"%s\", ", pt->ident);
		strncat(buffer2,buffer,4096);
	}

	snprintf(buffer,4096,"\"value\": %ju, ", (uintmax_t)val);
	strncat(buffer2,buffer,4096);

	snprintf(buffer,4096,"\"flag\": \"%c\", ", pt->flag);
	strncat(buffer2,buffer,4096);
	snprintf(buffer,4096,"\"description\": \"%s\"", pt->desc);
	strncat(buffer2,buffer,4096);
	snprintf(buffer,4096,"}");
	strncat(buffer2,buffer,4096);

	if (varnishstat->jp) {
		snprintf(buffer,4096,"\n");
		strncat(buffer2,buffer,4096);
	}
	varnishstat->buffer = realloc(varnishstat->buffer, (varnishstat->buffer ? strlen(varnishstat->buffer) : 0) + strlen(buffer2) + 1);
	strcat(varnishstat->buffer, buffer2);
	return (0);
}

static void
do_json(struct varnishstat_priv_t *varnishstat)
{
	char time_stamp[20];
	time_t now;
	int ret;
	varnishstat->jp = 1;
	now = time(NULL);

	(void)strftime(time_stamp, 20, "%Y-%m-%dT%H:%M:%S", localtime(&now));
	ret = asprintf(&(varnishstat->buffer),"{\n\t\"timestamp\": \"%s\",\n", time_stamp);
	assert(ret);
	(void)VSC_Iter(varnishstat->vd, do_json_cb, varnishstat);
	varnishstat->buffer = realloc(varnishstat->buffer, strlen(varnishstat->buffer) + 1 + 3);
	strcat(varnishstat->buffer, "\n}\n");
}

static unsigned int varnishstat_reply(struct httpd_request *request, void *data)
{
	struct varnishstat_priv_t *varnishstat;
	GET_PRIV(data,varnishstat);
	do_json(varnishstat);
	send_response(request->connection, 200, varnishstat->buffer, strlen(varnishstat->buffer));
	free(varnishstat->buffer);
	return 0;
}
void
varnishstat_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct varnishstat_priv_t *priv = malloc(sizeof(struct varnishstat_priv_t));
	int ret;
	plug = plugin_find(core,"varnishstat");
	assert(plug);
	
	priv->vd = VSM_New();
	assert(priv->vd);
	VSC_Setup(priv->vd);
	priv->jp = 0;
	priv->buffer = NULL;
	plug->data = priv;

	if (core->config->n_arg)
		VSC_Arg(priv->vd, 'n', core->config->n_arg);

	ret = VSC_Open(priv->vd, 1);
	assert(ret == 0);
	priv->VSC_C_main = VSC_Main(priv->vd);
	assert(priv->VSC_C_main);

	httpd_register_url(core, "/stats", M_GET, varnishstat_reply, core);
	return;
}

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

#define _GNU_SOURCE
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>

#include "common.h"
#include "ipc.h"
#include "http.h"
#include "helpers.h"
#include "plugins.h"
#include "vsb.h"

#define ID_LEN	10

struct vcl_priv_t {
	int logger;
	int vadmin;
	char *help;
};

struct vcl_list {
	char available[11];
	char state[5];
	char temp[5];
	char name[1024];
};

static void
mk_help(struct agent_core_t *core, struct vcl_priv_t *vcl)
{
	assert(0 < asprintf(&vcl->help, "The following logic can be used:\n"
		"GET /vcl/ - Fetch a list of VCLs (e.g: vcl.list)\n"
		"GET /vcl/vclname - Fetch the vcl named vclname (vcl.show)\n"
		"POST /vcl/ - Upload a new VCL, named dynamically. (vcl.inline).\n"
		"PUT /vcl/vclname - Upload a new VCL with the specified name.\n"
		"DELETE /vcl/vclname - Discard a named VCL (vcl.discard)\n"
		"GET /vclactive/ - Get the name of the active vcl config\n"
		"PUT /vcldeploy/vclname - Deploy the vcl (e.g: vcl.use)\n\n"
		"VCL is saved to '%s/<name>.auto.vcl'.\n"
		"A successful vcl.deploy through the agent will update\n"
		"'%s/boot.vcl'\n"
		"That way, you can start varnishd with the most recent VCL\n"
		"by using:\n"
		"\"varnishd (...) -f %s/boot.vcl\"\n",
		core->config->p_arg, core->config->p_arg, core->config->p_arg));
}

/*
 * Store VCL to disk if possible. Returns 0 if all went well, -1 on error.
 *
 */
static int
vcl_persist(int logfd, const char *id, const char *vcl, struct agent_core_t *core)
{
	char tempfile[PATH_MAX];
	char target[PATH_MAX];
	int fd = -1;

	snprintf(target, sizeof(target), "%s/%s.auto.vcl",
	    core->config->p_arg, id);
	snprintf(tempfile, sizeof(tempfile), "%s/.tmp.%s.auto.vcl",
	    core->config->p_arg, id);

	errno = 0;
	if (unlink(tempfile) < 0 && errno != ENOENT) {
		warnlog(logfd, "Removing temporary file '%s' failed: %s",
		    tempfile, strerror(errno));
		return (-1);
	}

	fd = open(tempfile, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, S_IRWXU);
	if (fd < 0) {
		warnlog(logfd, "Failed to open %s for writing: %s",
		    tempfile, strerror(errno));
		return (-1);
	}

	assert(write(fd, (const void *)vcl, strlen(vcl)) > 0);
	AZ(fsync(fd));
	AZ(close(fd));

	if (rename(tempfile, target)) {
		warnlog(logfd,
		    "rename of %s to %s failed."
		    " Dunno quite what to do. errno: %d(%s)",
		    tempfile, target, errno, strerror(errno));
		return (-1);
	}
	return (0);
}

/*
 * Set the link for the active VCL.
 */
static int
vcl_persist_active(int logfd, const char *id, struct agent_core_t *core)
{
	int ret;
	char buf[1024];
	char active[1024];
	char active_tmp[1024];
	struct stat sbuf;
	/*
	 * FIXME: need to move things into place to avoid disaster if we
	 * crash during update, leaving no active vcl in place.
	 */
	errno = 0;
	snprintf(buf, sizeof buf, "%s/%s.auto.vcl", core->config->p_arg, id);
	snprintf(active_tmp, sizeof active_tmp, "%s/boot.vcl.tmp",
	    core->config->p_arg);
	snprintf(active, sizeof active, "%s/boot.vcl", core->config->p_arg);

	ret = stat(buf, &sbuf);
	if (ret < 0) {
		warnlog(logfd, "Failed to stat() %s: %s", active,
		    strerror(errno));
		return (-1);
	}
	if (!(S_ISREG(sbuf.st_mode))) {
		warnlog(logfd, "%s is not a regular file?", active);
		return (-1);
	}

	ret = unlink(active_tmp);
	if (ret && errno != ENOENT) {
		warnlog(logfd, "Failed to unlink %s: %s", active_tmp,
		    strerror(errno));
		return (-1);
	}

	ret = link(buf, active_tmp);
	if (ret != 0) {
		warnlog(logfd, "Failed to link %s->%s: %s", buf, active_tmp,
		    strerror(errno));
		return (-1);
	}

	ret = rename(active_tmp, active);
	if (ret) {
		warnlog(logfd,
		    "rename of %s to %s failed."
		    " Dunno quite what to do. errno: %d(%s)",
		    active_tmp, active, errno, strerror(errno));
		return (-1);
	}

	return (0);
}

/* Check if something is a valid C identifier. */

static int
valid_c_ident(const char *ident)
{
	while (*ident != '\0') {
		if (!isalnum(*ident) && *ident != '_')
			return (0);
		ident++;
	}
	return (1);
}

static int
vcl_store(struct http_request *request, struct vcl_priv_t *vcl,
	struct ipc_ret_t *vret, struct agent_core_t *core, const char *id)
{
	int ret;
	assert(request->data);
	if (request->ndata == 0) {
		warnlog(vcl->logger, "vcl.inline with ndata == 0");
		ANSWER(vret, 400, "No VCL found");
		return (500);
	}
	assert(request->ndata > 0);
	assert(id);
	assert(strlen(id)>0);

	if (! valid_c_ident(id)) {
		ANSWER(vret, 400, "VCL name is not valid");
		return (500);
	}
	const char *end = (((char*)request->data)[request->ndata-1] == '\n') ? "" : "\n";

	ipc_run(vcl->vadmin, vret,
	    "vcl.inline %s << __EOF_%s__\n%s%s__EOF_%s__",
	    id, id, (char *)request->data, end, id);
	if (vret->status == 200) {
		logger(vcl->logger, "VCL stored OK");
		ret = vcl_persist(vcl->logger, id, request->data, core);
		if (ret) {
			warnlog(vcl->logger,
			    "vcl.inline OK, but persisting to disk failed."
			    " Errno: %d", errno);
			free(vret->answer);
			vret->answer = strdup(
			   "VCL stored in varnish OK,"
			   " but persisting to disk failed.");
			return (500);
		}
		return (201);
	} else {
		warnlog(vcl->logger, "vcl.inline failed");
		return (500);
	}
}

/*
 * Takes unformatted vcl.list as input and returns a vsb with the json
 * version. The caller must clean up the vsb.
 */
static struct vsb *
vcl_list_json(char *raw)
{
	struct vcl_list tmp;
	int ret;
	char *pos;
	char ref_temp[10];
	struct vsb *vsb;
	vsb = VSB_new_auto();
	pos = raw;
	VSB_printf(vsb,"{\n\t\"vcls\": [\n");
	while (1) {
		assert(strlen(pos) > 30);
		if (pos[30] != ' ')
			ret = sscanf(pos, "%10s %4s/%4s  %6s %s\n",
			    tmp.available, tmp.state, tmp.temp, ref_temp,
			    tmp.name);
		else
			ret = sscanf(pos, "%10s %4s/%4s   %s\n",
			    tmp.available, tmp.state, tmp.temp, tmp.name);
		if (ret <= 0) {
			/*
			 * FIXME: This should go into the logger
			 */
			printf("Confused! line: %s\n", pos);
			VSB_clear(vsb);
			return (vsb);
		}
		assert(ret>0);
		VSB_printf(vsb,
			"\t\t%s{\n"
			"\t\t\t\"name\": \"%s\",\n"
			"\t\t\t\"status\": \"%s\",\n"
			"\t\t\t\"temp\": \"%s\",\n"
			"\t\t\t\"mode\": \"%s\"\n"
			"\t\t}",
			pos != raw ? ",\n" : "", tmp.name, tmp.available,
			tmp.temp, tmp.state);

		pos = strstr(pos,"\n");
		if (pos == NULL)
			break;
		pos += 1;
		if (pos[0] == '\0' || pos[0] == '\n')
			break;
	}
	VSB_printf(vsb,"\n\t]\n}\n");
	return vsb;
}

static unsigned int
vcl_json(struct http_request *request, const char *arg, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct ipc_ret_t vret;
	struct vsb *json;
	struct http_response *resp;

	GET_PRIV(core, vcl);

	assert(STARTS_WITH(request->url, "/vcljson/"));
	assert(request->method == M_GET);

	if (arg) {
		http_reply(request->connection, 404,
		    "/vcljson takes no argument");
		return (0);
	}

	ipc_run(vcl->vadmin, &vret, "vcl.list");
	if (vret.status == 400)
		http_reply(request->connection, 500, vret.answer);
	else {
		json = vcl_list_json(vret.answer);
		assert(VSB_finish(json) == 0);
		resp = http_mkresp(request->connection, 200, NULL);
		resp->data = VSB_data(json);
		resp->ndata = VSB_len(json);
		http_add_header(resp, "Content-Type", "application/json");
		send_response(resp);
		http_free_resp(resp);
		VSB_clear(json);
		VSB_delete(json);
	}
	free(vret.answer);
	return (0);
}


static unsigned int
vcl_listshow(struct http_request *request, const char *arg, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct ipc_ret_t vret;

	GET_PRIV(core, vcl);

	assert(STARTS_WITH(request->url, "/vcl/"));
	assert(request->method == M_GET);

	if (!arg)
		ipc_run(vcl->vadmin, &vret, "vcl.list");
	else
		ipc_run(vcl->vadmin, &vret, "vcl.show %s", arg);

	if (vret.status == 200)
		http_reply(request->connection, 200, vret.answer);
	else
		http_reply(request->connection, 400, vret.answer);

	free(vret.answer);
	return (0);
}

static unsigned int
vcl_push(struct http_request *request, const char *arg, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct ipc_ret_t vret;
	char id[ID_LEN + 3 + 1];
	int status;

	GET_PRIV(core, vcl);

	assert(STARTS_WITH(request->url, "/vcl/"));
	assert(request->method == M_POST || request->method == M_PUT);

	if (request->method == M_POST)
		snprintf(id, sizeof(id), "vcl%ju", (uintmax_t) time(NULL));
	else
		snprintf(id, sizeof(id), "%s", arg);

	if (!strlen(id))
		http_reply(request->connection, 400, "Bad URL?");
	else {
		status = vcl_store(request, vcl, &vret, core, id);
		http_reply(request->connection, status, vret.answer);
		free(vret.answer);
	}
	return (0);
}

static unsigned int
vcl_delete(struct http_request *request, const char *arg, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct ipc_ret_t vret;

	GET_PRIV(core, vcl);

	assert(request->method == M_DELETE);
	assert(STARTS_WITH(request->url, "/vcl/"));

	ipc_run(vcl->vadmin, &vret, "vcl.discard %s", arg);
	if (vret.status == 400 || vret.status == 106)
		http_reply(request->connection, 500, vret.answer);
	else
		http_reply(request->connection, 200, vret.answer);
	free(vret.answer);
	return (0);
}

static unsigned int
vcl_active(struct http_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct ipc_ret_t vret;
	char **tp, *tok[5];
	char *p, *last;
	char *line;

	assert(STARTS_WITH(request->url, "/vclactive/"));
	assert(request->method == M_GET);
	GET_PRIV(core, vcl);

	/*
	 * vcl.list output:
	 *
	 * V3/4 : (active|available|discarded) (refcnt) (name)
	 * V4.1 : (active|available|discarded) (state) \
	 *            (busycnt|) (name)
	 */
	ipc_run(vcl->vadmin,&vret,"vcl.list");
	if (vret.status == 400) {
		http_reply(request->connection, 500, vret.answer);
		free(vret.answer);
		return (0);
	}
	memset(tok, '\0', sizeof(tok));
	for (p = vret.answer, last = NULL;
	    (line = strtok_r(p, "\n", &last)); p = NULL) {
		if (strncmp("active", line, 6))
			continue;
		last = NULL;
		for (p = line, tp = tok;
		    tp < &tok[4] && (*tp = strtok_r(p, " ", &last)); p = NULL) {
			if (**tp != '\0')
				tp++;
		}
	}
	if (!tok[2] || !tok[3])
		http_reply(request->connection, 500, "No active VCL");
	else {
		strcpy(vret.answer, tok[3] ? tok[3] : tok[2]);
		http_reply(request->connection, 200, vret.answer);
	}
	free(vret.answer);
	return (0);
}

static unsigned int
vcl_deploy(struct http_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct ipc_ret_t vret;
	int ret;

	GET_PRIV(core, vcl);

	assert(STARTS_WITH(request->url, "/vcldeploy/"));
	assert(request->method == M_PUT);

	ipc_run(vcl->vadmin, &vret, "vcl.use %s",
	    url_arg(request->url, "/vcldeploy/"));
	if (vret.status == 200) {
		ret = vcl_persist_active(vcl->logger,
		    url_arg(request->url, "/vcldeploy/"), core);
	}
	if (vret.status == 200 && ret)
		http_reply(request->connection, 500,
		    "Deployed ok, but NOT PERSISTED.");
	else if (vret.status == 200)
		http_reply(request->connection, 200, vret.answer);
	else
		http_reply(request->connection, 500, vret.answer);
	free(vret.answer);
	return (0);
}

void
vcl_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vcl_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vcl");
	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	mk_help(core, priv);
	http_register_url2(core, "/vcljson/", M_GET, vcl_json, core);
	http_register_url2(core, "/vcl/", M_GET, vcl_listshow, core);
	http_register_url2(core, "/vcl/", M_PUT | M_POST, vcl_push, core);
	http_register_url2(core, "/vcl/", M_DELETE, vcl_delete, core);
	http_register_url(core, "/vclactive", M_GET , vcl_active, core);
	http_register_url(core, "/vcldeploy/", M_PUT , vcl_deploy, core);
	http_register_url(core, "/help/vcl", M_GET, help_reply, priv->help);
}

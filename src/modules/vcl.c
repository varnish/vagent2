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
#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "http.h"
#include "vsb.h"
#include "helpers.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define VCL_PATH "tmp/"
#define VCL_ACTIVE_PATH "tmp/active_vcl"


struct vcl_priv_t {
	int logger;
	int vadmin;
	char *help;
};

struct vcl_list {
	char name[1024];
	char available[11];
	char ref[11];
};

static void mk_help(struct agent_core_t *core, struct vcl_priv_t *vcl)
{
	int ret;
	ret = asprintf(&vcl->help, "The following logic can be used:\n"
		"GET /vcl/ - Fetch a list of VCLs (e.g: vcl.list)\n"
		"GET /vcl/vclname - Fetch the vcl named vclname (vcl.show)\n"
		"POST /vcl/ - Upload a new VCL, named dynamically. (vcl.inline).\n"
		"PUT /vcl/vclname - Upload a new VCL with the specified name.\n"
		"DELETE /vcl/vclname - Discard a named VCL (vcl.discard)\n"
		"PUT /vcldeploy/vclname - Deploy the vcl (e.g: vcl.use)\n\n"
		"VCL is saved to '%s/<name>.auto.vcl'.\n"
		"A successful vcl.deploy through the agent will update\n"
		"'%s/boot.vcl'\n"
		"That way, you can start varnishd with the most recent VCL\n"
		"by using:\n"
		"\"varnishd (...) -f %s/boot.vcl\"\n",
		core->config->p_arg, core->config->p_arg, core->config->p_arg);
	assert(ret>0);
}

/*
 * Store VCL to disk if possible. Returns 0 if all went well, -1 on error.
 *
 */
static int vcl_persist(int logfd, const char *id, const char *vcl, struct agent_core_t *core) {
	int ret;
	_cleanup_free_ char *tmpfile = NULL;
	_cleanup_free_ char *target = NULL;
	_cleanup_close_ int fd = -1;

	ret = asprintf(&target, "%s/%s.auto.vcl", core->config->p_arg, id);
	assert(ret > 0);

	ret = asprintf(&tmpfile, "%s/.tmp.%s.auto.vcl", core->config->p_arg, id);
	assert(ret > 0);

	ret = unlink(tmpfile);
	if (ret < 0 && errno != ENOENT) {
		warnlog(logfd, "Removing temporary file '%s' failed: %s", tmpfile, strerror(errno));
		return -1;
	}

	fd = open(tmpfile, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, S_IRWXU);
	if (fd < 0) {
		warnlog(logfd, "Failed to open %s for writing: %s", tmpfile, strerror(errno));
		return -1;
	}

	ret = write(fd, (const void *)vcl, strlen(vcl));
	assert(ret > 0);
	ret = fsync(fd);
	assert(ret >= 0);
	ret = close(fd);
	assert(ret >= 0);
	fd = -1;
	ret = rename(tmpfile, target);
	if (ret) {
		warnlog(logfd, "rename of %s to %s failed. Dunno quite what to do. errno: %d(%s)", tmpfile, target, errno, strerror(errno));
		return -1;
	}
	return 0;
}

/*
 * Set the link for the active VCL.
 */
static int vcl_persist_active(int logfd, const char *id, struct agent_core_t *core)
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
	snprintf(buf, sizeof buf, "%s/%s.auto.vcl", core->config->p_arg, id);
	snprintf(active_tmp, sizeof active_tmp, "%s/boot.vcl.tmp", core->config->p_arg);
	snprintf(active, sizeof active, "%s/boot.vcl", core->config->p_arg);

	ret = stat(buf, &sbuf);
	if (ret < 0) {
		warnlog(logfd, "Failed to stat() %s: %s", active, strerror(errno));
		return -1;
	}
	if (!(S_ISREG(sbuf.st_mode))) {
		warnlog(logfd, "%s is not a regular file?", active);
		return -1;
	}

	ret = unlink(active_tmp);
	if (ret && errno != ENOENT) {
		warnlog(logfd, "Failed to unlink %s: %s", active_tmp, strerror(errno));
		return -1;
	}

	ret = link(buf, active_tmp);
	if (ret != 0) {
		warnlog(logfd, "Failed to link %s->%s: %s", buf, active_tmp, strerror(errno));
		return -1;
	}

	ret = rename(active_tmp, active);
	if (ret) {
		warnlog(logfd, "rename of %s to %s failed. Dunno quite what to do. errno: %d(%s)", active_tmp, active, errno, strerror(errno));
		return -1;
	}

	return 0;
}

/* Check if something is a valid C identifier. */

static int valid_c_ident(const char *ident)
{
	while (*ident != '\0') {
		if (!isalnum(*ident) && *ident != '_')
			return 0;
		ident++;
	}
	return 1;
}

static int vcl_store(struct http_request *request,
		      struct vcl_priv_t *vcl,
		      struct ipc_ret_t *vret,
		      struct agent_core_t *core,
		      const char *id)
{
	int ret;
	assert(request->data);
	if (request->ndata == 0) {
		warnlog(vcl->logger, "vcl.inline with ndata == 0");
		vret->status = 400;
		vret->answer = strdup("No VCL found");
		return 500;
	}
	assert(request->ndata > 0);
	assert(id);
	assert(strlen(id)>0);

	if (! valid_c_ident(id)) {
		vret->status = 400;
		vret->answer = strdup("VCL name is not valid");
		return 500;
	}
	const char *end = (((char*)request->data)[request->ndata-1] == '\n') ? "" : "\n";

	ipc_run(vcl->vadmin, vret, "vcl.inline %s << __EOF_%s__\n%s%s__EOF_%s__",
		id,id,(char *)request->data,end,id);
	if (vret->status == 200) {
		logger(vcl->logger, "VCL stored OK");
		ret = vcl_persist(vcl->logger, id, request->data, core);
		if (ret) {
			warnlog(vcl->logger, "vcl.inline OK, but persisting to disk failed. Errno: %d", errno);
			free(vret->answer);
			vret->answer = strdup("VCL stored in varnish OK, but persisting to disk failed.");
			return 500;
		}
		return 201;
	} else {
		warnlog(vcl->logger, "vcl.inline failed");
		return 500;
	}
}

/*
 * Takes unformatted vcl.list as input and returns a vsb with the json
 * version. The caller must clean up the vsb.
 */
static struct vsb *vcl_list_json(char *raw)
{
	struct vcl_list tmp;
	int ret;
	char *pos;
	struct vsb *vsb;
	vsb = VSB_new_auto();
	pos = raw;
	VSB_printf(vsb,"{\n\t\"vcls\": [\n");
	do {
		ret = sscanf(pos, "%10s %6s %s\n", tmp.available, tmp.ref, tmp.name);
		if (ret <= 0) {
			/*
			 * FIXME: This should go into the logger
			 */
			printf("Confused! line: %s\n", pos);
			VSB_clear(vsb);
			return vsb;
		}
		assert(ret>0);
		VSB_printf(vsb, "%s{\n"
			"\t\t\t\"name\": \"%s\",\n"
			"\t\t\t\"status\": \"%s\",\n"
			"\t\t\t\"refs\": \"%s\"\n"
			"\t\t}",pos != raw ? ",\n\t\t" : "\t\t",
			tmp.name, tmp.available, tmp.ref);

		pos = strstr(pos,"\n");
		if (pos == NULL)
			break;
		pos+=1;
		if (pos[0] == '\0' || pos[0] == '\n')
			break;
	} while (1);
	VSB_printf(vsb,"\n\t]\n}\n");
	return vsb;
}

static unsigned int vcl_reply(struct http_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct vcl_priv_t *vcl;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;
	char *cmd;
	int ret;
	int status;

	assert(core);

	plug = plugin_find(core,"vcl");
	assert(plug);

	vcl = plug->data;
	assert(vcl);

	if (request->method == M_GET) {
		if (!strcmp(request->url, "/vcl") || !strcmp(request->url,"/vcl/")) {
			ipc_run(vcl->vadmin, &vret, "vcl.list");
			if (vret.status == 400) {
				send_response_fail(request->connection, vret.answer);
			} else {
				send_response_ok(request->connection, vret.answer);
			}
			free(vret.answer);
			return 0;
		} else if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.show %s", request->url + strlen("/vcl/"));
			if (vret.status == 400) {
				send_response_fail(request->connection, vret.answer);
			} else {
				send_response_ok(request->connection, vret.answer);
			}
			free(vret.answer);
			return 0;
		} else if(!strcmp(request->url, "/vcljson/")) {
			struct vsb *json;
			ipc_run(vcl->vadmin, &vret, "vcl.list");
			if (vret.status == 400) {
				send_response_fail(request->connection, vret.answer);
			} else {
				json = vcl_list_json(vret.answer);
				assert(VSB_finish(json) == 0);
				struct http_response *resp = http_mkresp(request->connection, 200, NULL);
				resp->data = VSB_data(json);
				resp->ndata = VSB_len(json);
				http_add_header(resp, "Content-Type", "application/json");
				send_response2(resp);
				http_free_resp(resp);
				VSB_clear(json);
				VSB_delete(json);
			}
			free(vret.answer);
			return 0;
		} else {
			send_response_fail(request->connection, "Invalid VCL-url.");
			return 0;
		}
	} else if (request->method == M_POST) {
		ret = asprintf(&cmd, "%d", (unsigned int)time(NULL));
		assert(ret>0);
		status = vcl_store(request, vcl, &vret, core, cmd);
		free(cmd);
		struct http_response *resp = http_mkresp(request->connection, status, vret.answer);
		send_response2(resp);
		http_free_resp(resp);
		free(vret.answer);
		return 0;
	} else if (request->method == M_PUT) {
		if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) {
			if (strlen(request->url) >= 6) {
				status = vcl_store(request, vcl, &vret, core,
				                   request->url + strlen("/vcl/"));
				struct http_response *resp = http_mkresp(request->connection, status, vret.answer);
				send_response2(resp);
				http_free_resp(resp);
				free(vret.answer);
				return 0;
			} else {
				struct http_response *resp = http_mkresp(request->connection, 400, "Bad URL?");
				send_response2(resp);
				http_free_resp(resp);
				return 0;
			}
		} else if (!strncmp(request->url, "/vcldeploy/",strlen("/vcldeploy/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.use %s",
				request->url + strlen("/vcldeploy/"));
			if (vret.status == 200) {
				ret = vcl_persist_active(vcl->logger, request->url + strlen("/vcldeploy/"), core);
			}
			if (vret.status == 200 && ret)
				send_response_fail(request->connection, "Deployed ok, but NOT PERSISTED.");
			else if (vret.status == 200 && ret == 0)
				send_response_ok(request->connection, vret.answer);
			else
				send_response_fail(request->connection, vret.answer);
			free(vret.answer);
			return 0;
		}
	} else if (request->method == M_DELETE) {
		if (!strncmp(request->url, "/vcl/", strlen("/vcl/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.discard %s",
				request->url + strlen("/vcl/"));
			if (vret.status == 400 || vret.status == 106) {
				send_response_fail(request->connection, vret.answer);
			} else {
				send_response_ok(request->connection, vret.answer);
			}
			free(vret.answer);
			return 0;
		}
	} else {
		return send_response_fail(request->connection, "Unknown request?");
	}
	assert("Shouldn't get here" == NULL);
	return 0;
}

void vcl_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vcl_priv_t *priv = malloc(sizeof(struct vcl_priv_t));
	plug = plugin_find(core,"vcl");

	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
	mk_help(core, priv);
	http_register_url(core, "/vcljson/", M_GET, vcl_reply, core);
	http_register_url(core, "/vcl/", M_DELETE | M_PUT | M_GET | M_POST, vcl_reply, core);
	http_register_url(core, "/vcldeploy/", M_PUT , vcl_reply, core);
	http_register_url(core, "/help/vcl", M_GET, help_reply, priv->help);
}

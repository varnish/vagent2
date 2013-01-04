/*
 * Copyright (c) 2012 Varnish Software AS
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
#include "httpd.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define VCL_PATH "tmp/"
#define VCL_ACTIVE_PATH "tmp/active_vcl"


struct vcl_priv_t {
	int logger;
	int vadmin;
	const char *help;
};

struct vcl_list {
	char name[1024];
	char available[11];
	char ref[11];
};

static void mk_help(struct vcl_priv_t *vcl) {
	vcl->help = "The following logic can be used:\n"
		"GET /vcl/ - Fetch a list of VCLs (e.g: vcl.list)\n"
		"GET /vcl/vclname - Fetch the vcl named vclname (vcl.show)\n"
		"POST /vcl/ - Upload a new VCL, named dynamically. (vcl.inline).\n"
		"PUT /vcl/vclname - Upload a new VCL with the specified name.\n"
		"DELETE /vcl/vclname - Discard a named VCL (vcl.discard)\n"
		"PUT /vcldeploy/vclname - Deploy the vcl (e.g: vcl.use)\n";
}

static int vcl_persist(const char *id, const char *vcl) {
	int fd, ret;
	char *path;
	struct stat sbuf;
	fd = asprintf(&path, VCL_PATH "%s.vcl", id);
	assert(fd>0);
	fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
	if (fd < 0) {
		ret = -1;
		goto out;
	}
	ret = stat(path, &sbuf);
	if (ret < 0) {
		ret = -1;
		goto out;
	}
	if (!(S_ISREG(sbuf.st_mode))) {
		ret = -1;
		goto out;
	}
	ret = write(fd, (const void *)vcl, strlen(vcl));
	assert(ret>0);
	ret = 0;
	out:
	close(fd);
	free(path);
	return ret;
}

static int vcl_persist_active(const char *id)
{
	int ret;
	char buf[1024];
	struct stat sbuf;
	/*
	 * FIXME: need to move things into place to avoid disaster if we
	 * crash during update, leaving no active vcl in place.
	 */
	sprintf(buf, VCL_PATH "%s.vcl", id);
	ret = stat(buf, &sbuf);
	if (ret < 0)
		return -1;
	if (!(S_ISREG(sbuf.st_mode)))
		return -1;
	unlink(VCL_ACTIVE_PATH);
	sprintf(buf, "%s.vcl", id);
	ret = symlink(buf, VCL_ACTIVE_PATH);
	if (ret!=0)
		return -1;
	return 0;
}

static int vcl_store(struct httpd_request *request,
		      struct vcl_priv_t *vcl,
		      struct ipc_ret_t *vret,
		      const char *id)
{
	int ret;
	ipc_run(vcl->vadmin, vret, "vcl.inline %s << __EOF_%s__\n%s\n__EOF_%s__",
		id,id,(char *)request->data,id);
	if (vret->status == 200) {
		logger(vcl->logger, "VCL stored OK");
		ret = vcl_persist(id, request->data);
		if (ret) {
			logger(vcl->logger, "vcl.inline OK, but persisting to disk failed. Errno: %d", errno);
			free(vret->answer);
			vret->answer = strdup("VCL stored in varnish OK, but persisting to disk failed.");
			return 500;
		}
		return 201;
	} else {
		logger(vcl->logger, "vcl.inline failed");
		return 500;
	}
}

static char *vcl_list_json(char *raw)
{
	struct vcl_list *tmp;
	int ret, ret2;
	char *pos;
	char *buffer = calloc(1,10241);
	char *b;
	pos = raw;
	tmp = malloc(sizeof (struct vcl_list));
	strcat(buffer,"{\n\"vcls\": [\n");
	do {
		ret = sscanf(pos, "%10s %6s %s\n", tmp->available, tmp->ref, tmp->name);
		if (ret == 0) {
			printf("Confused! line: %s\n", pos);
		}
		assert(ret>0);
		ret2 = asprintf(&b, "%s{\n"
			"\t\"name\": \"%s\",\n"
			"\t\"status\": \"%s\",\n"
			"\t\"refs\": \"%s\"\n"
			"}",pos != raw ? ",\n" : "",
			tmp->name, tmp->available, tmp->ref);
		assert(ret2>0);
		strncat(buffer,b,10240);
		free(b);
		pos = strstr(pos,"\n");
		if (pos == NULL)
			break;
		pos+=1;
		if (pos[0] == '\0' || pos[0] == '\n')
			break;
	} while (1);
	strncat(buffer,"]\n}\n",10240);
	return buffer;
}

static unsigned int vcl_reply(struct httpd_request *request, void *data)
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
			return send_response_ok(request->connection, vret.answer);
		} else if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.show %s", request->url + strlen("/vcl/"));
			return send_response_ok(request->connection, vret.answer);
		} else if(!strncmp(request->url, "/help/vcl", strlen("/help/vcl"))) {
			return send_response_ok(request->connection, vcl->help);
		} else if(!strcmp(request->url, "/vcljson/")) {
			ipc_run(vcl->vadmin, &vret, "vcl.list");
			vret.answer = vcl_list_json(vret.answer);
			return send_response_ok(request->connection, vret.answer);
		}
	} else if (request->method == M_POST) {
		ret = asprintf(&cmd, "%d", (unsigned int)time(NULL));
		assert(ret>0);
		status = vcl_store(request, vcl, &vret, cmd);
		free(cmd);
		return send_response(request->connection,status, vret.answer, strlen(vret.answer));
	} else if (request->method == M_PUT) {
		if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) {
			if (strlen(request->url) >= 6) {
				status = vcl_store(request, vcl, &vret,
					           request->url + strlen("/vcl/"));
				return send_response(request->connection,status,
						     vret.answer, strlen(vret.answer));
			}
		} else if (!strncmp(request->url, "/vcldeploy/",strlen("/vcldeploy/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.use %s",
				request->url + strlen("/vcldeploy/"));
			if (vret.status == 200) {
				ret = vcl_persist_active(request->url + strlen("/vcldeploy/"));
			}
			if (vret.status == 200 && ret)
				return send_response_fail(request->connection, "Deployed ok, but NOT PERSISTED.");
			if (vret.status == 200 && ret == 0)
				return send_response_ok(request->connection, vret.answer);
			return send_response_fail(request->connection, vret.answer);

		}
	} else if (request->method == M_DELETE) {
		if (!strncmp(request->url, "/vcl/", strlen("/vcl/"))) {
			ipc_run(vcl->vadmin, &vret, "vcl.discard %s",
				request->url + strlen("/vcl/"));
			return send_response_ok(request->connection, vret.answer);
		}
	} else {
		return send_response_fail(request->connection, "Unknown request?");
	}
	assert("Shouldn't get here");
	return 0;
}

void vcl_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vcl_priv_t *priv = malloc(sizeof(struct vcl_priv_t));
	plug = plugin_find(core,"vcl");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
	httpd_register_url(core, "/vcljson/", M_GET, vcl_reply, core);
	httpd_register_url(core, "/vcl/", M_DELETE | M_PUT | M_GET | M_POST, vcl_reply, core);
	httpd_register_url(core, "/vcldeploy/", M_PUT , vcl_reply, core);
	httpd_register_url(core, "/help/vcl",  M_GET , vcl_reply, core);
	mk_help(priv);
}

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "http.h"
#include "ipc.h"
#include "plugins.h"


struct html_priv_t {
	int logger;
};

static unsigned int
html_reply(struct http_request *request, void *data)
{
	int ret;
	int fd;
	char path[PATH_MAX];
	char *buffer;
	struct stat sbuf;
	struct agent_core_t *core = data;
	struct http_response *resp;
	struct html_priv_t *html;

	GET_PRIV(core, html);
	const char *url_stub = (strlen(request->url) > strlen("/html/")) ? request->url + strlen("/html/") : "index.html";
	if (strlen(request->url) == strlen("/html")) {
		char *host_header = http_get_header(request->connection,"Host");
		char *tmp = NULL;
		resp = http_mkresp(request->connection, 301, NULL);
		if (host_header == NULL) {
			logger(html->logger, "Requested /html but no Host header found. Can't redirect correctly.");
			http_add_header(resp, "Location", "/html/");
		} else {
			ret = asprintf(&tmp, "http://%s/html/", host_header);
			assert(ret);
			assert(tmp);
			http_add_header(resp,"Location",tmp);
		}
		send_response(resp);
		http_free_resp(resp);
		if (tmp)
			free(tmp);
		return 0;
	}
	if (url_stub[0] == '/' || strstr(url_stub,"/../") ||
			STARTS_WITH(url_stub, "../")) {
		http_reply(request->connection, 500, "Invalid URL");
		return 0;
	}
	snprintf(path, sizeof(path), "%s/%s", core->config->H_arg, url_stub);
	ret = stat(path, &sbuf);
	if (ret < 0) {
		warnlog(html->logger, "Stat failed for %s. Errnno %d: %s.", path,errno,strerror(errno));
		http_reply(request->connection, 500, "stat() was not happy");
		return 0;
	}
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		warnlog(html->logger, "open() failed for %s: %s", path, strerror(errno));
		http_reply(request->connection, 500, "open() was not happy");
		return 0;
	}
	if (!S_ISREG(sbuf.st_mode)) {
		warnlog(html->logger, "%s isn't a regular file.", path);
		http_reply(request->connection, 500, "not a file");
		close(fd);
		return 0;
	}
	buffer = malloc(sbuf.st_size);
	assert(buffer);
	ret = read(fd, buffer, sbuf.st_size);
	assert(ret==sbuf.st_size);
	close(fd);
	resp = http_mkresp(request->connection, 200, NULL);
	resp->data = buffer;
	resp->ndata = ret;
	http_set_content_type(resp, path);
	send_response(resp);
	http_free_resp(resp);
	free(buffer);
	return 0;
}

void
html_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct html_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"html");
	priv->logger = ipc_register(core,"logger");
	plug->data = (void *)priv;
	http_register_url(core, "/html", M_GET, html_reply, core);
}

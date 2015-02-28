/*
 * Copyright (c) 2012-2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Yves Hwang <yveshwang@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "common.h"
#include "ipc.h"
#include "plugins.h"

struct curl_priv_t {
	int logger;
	void *data;
	char *pos;
	unsigned int ndata;
};

static size_t
dropdata(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void)ptr;
	(void)userdata;
	return (size * nmemb);
}

static size_t
senddata(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct curl_priv_t *private = userdata;
	size_t len;

	len = 0;
	if (private->ndata != 0) {
		assert(private->pos);
		if (size * nmemb > private->ndata) {
			memcpy(ptr, (char *)private->pos, private->ndata);
			len = private->ndata;
			private->ndata = 0;
			private->pos = NULL;
		} else {
			memcpy(ptr, private->pos, size * nmemb);
			len = size * nmemb;
			private->ndata -= len;
			private->pos += len;
		}
	}
	return (len);
}

static void
issue_curl(void *priv, char *url, struct ipc_ret_t *ret)
{
	struct curl_priv_t *private = priv;
	struct curl_slist *slist = NULL;
	CURL *curl;
	CURLcode res;
	char buf[100];
	char *data;

	if (url == NULL || *url == '\0') {
		ANSWER(ret, 500, "VAC url is not supplied. "
		    "Please do so with the -z argument.");
		return;
	}

	data = strchr(url, '\n');
	if (data) {
		*data = '\0';
		private->data = data + 1;
		private->ndata = strlen(private->data);
		private->pos = private->data;
		snprintf(buf, sizeof(buf), "Content-Length: %u",
		    private->ndata);
		slist = curl_slist_append(slist, "expect:");
		slist = curl_slist_append(slist, buf);
		slist = curl_slist_append(slist, "Transfer-Encoding:");
	}

	debuglog(private->logger, "Issuing curl command with url=%s. %s",
	    url, data ? "Request body present" : "No request body");

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dropdata);
		if (data) {
			curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, senddata);
			curl_easy_setopt(curl, CURLOPT_READDATA, private);
		} else
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			snprintf(buf, sizeof(buf),
			    "Curl callback failed with status code %d", res);
			ANSWER(ret, 500, buf);
			warnlog(private->logger, "%s", ret->answer);
		} else {
			ANSWER(ret, 200, "OK");
		}
		curl_slist_free_all(slist);
		curl_easy_cleanup(curl);
	} else {
		ANSWER(ret, 500, "Unable to instantiate libcurl.");
		warnlog(private->logger, "%s", ret->answer);
	}
}

void
curl_init(struct agent_core_t *core)
{
	struct curl_priv_t *priv;
	struct agent_plugin_t *plug;

	curl_global_init(CURL_GLOBAL_ALL);

	ALLOC_OBJ(priv);
	plug = plugin_find(core, "curl");
	priv->logger = ipc_register(core, "logger");
	plug->data = (void *)priv;
	plug->start = ipc_start;
	plug->ipc->priv = priv;
	plug->ipc->cb = issue_curl;
}

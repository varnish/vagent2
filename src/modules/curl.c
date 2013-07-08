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

#define _GNU_SOURCE
#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "http.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>

struct curl_priv_t {
	int logger;
	void *data;
	char *pos;
	unsigned int ndata;
};

static size_t dropdata(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void)ptr;
	(void)userdata;
	return size*nmemb;
}

static size_t senddata( void *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct curl_priv_t *private = userdata;
	unsigned int tmp;
	if (private->ndata == 0) {
		tmp = 0;
	} else {
		assert(private->pos);
		if (size*nmemb > private->ndata) {
			memcpy(ptr, (char *) private->pos, private->ndata);
			tmp = private->ndata;
			private->ndata = 0;
			private->pos = NULL;
		} else {
			memcpy(ptr, private->pos, size*nmemb);
			tmp = size*nmemb;
			private->ndata -= tmp;
			private->pos += tmp;
		}
	}
	return tmp;
}

static void issue_curl(void *priv, char *url, struct ipc_ret_t *ret)
{
	struct curl_priv_t *private = priv;
	CURL *curl;
	CURLcode res;
	struct curl_slist *slist=NULL;
	void *data = NULL;
	char *c_length = NULL;
	int asnret;

	if( url == NULL || url[0] == '\0') {
		ret->answer = strdup("VAC url is not supplied. Please do so with the -z argument.");
		ret->status = 500;
		return;
	}

	data = index(url,'\n');
	if (data) {
		*(char *)data = '\0';
		private->data = (char *)data + 1;
		private->ndata = strlen(private->data);
		private->pos = private->data;
		asnret = asprintf(&c_length, "Content-Length: %u", private->ndata);
		assert(asnret);
		slist = curl_slist_append(slist, "expect:");
		slist = curl_slist_append(slist, c_length);
		slist = curl_slist_append(slist, "Transfer-Encoding:");
	}


	debuglog(private->logger, "Issuing curl command with url=%s. %s", url,
		 data ? "Request body present" : "No request body");

	curl = curl_easy_init();

	if(curl) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dropdata);
		if (data) {
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, senddata);
			curl_easy_setopt(curl, CURLOPT_READDATA, private);
		}
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			asnret = asprintf(&ret->answer,"Curl callback failed with status code %d", res);
			assert(asnret > 0);
			ret->status = 500;
			warnlog( private->logger, "%s", ret->answer);
		} else {
			ret->status = 200;
			ret->answer = strdup("OK");
		}
		curl_easy_cleanup(curl);
	} else {
		ret->answer = strdup("Unable to instantiate libcurl.");
		ret->status = 500;
		warnlog( private->logger, "%s", ret->answer);
	}
	free(c_length);
}

void curl_init( struct agent_core_t *core)
{
	struct curl_priv_t *private = malloc(sizeof(struct curl_priv_t));
	struct agent_plugin_t *plug;
	plug = plugin_find( core, "curl");
	assert(plug);
	curl_global_init(CURL_GLOBAL_ALL);
	
	private->logger = ipc_register(core, "logger");
	plug->data = (void *) private;
	plug->start  = ipc_start;
	plug->ipc->priv = private;
	plug->ipc->cb = issue_curl;
}

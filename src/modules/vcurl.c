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

#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>

struct vcurl_priv_t {
	int logger;
};

static void issue_curl(void *priv, char *url, struct ipc_ret_t *ret) {
	struct vcurl_priv_t *private = priv;
	CURL *curl;
	CURLcode res;
	logger( private->logger, "issuing curl command with url=%s", url);
	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1); //does a HEAD for now
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			ret->answer = strdup("Something went wrong. Incorrect URL?");
			ret->status = 500;
			logger( private->logger, "%s", ret->answer);
		} else {
			curl_easy_cleanup(curl);
			ret->status = 200;
			ret->answer = strdup("OK");
		}
	}
	else {
		ret->answer = strdup("Unable to instantiate libcurl.");
		ret->status = 500;
		logger( private->logger, "%s", ret->answer);
	}
}

void vcurl_init( struct agent_core_t *core) {
	struct vcurl_priv_t *private  =  malloc( sizeof(struct vcurl_priv_t) ) ;	
	struct agent_plugin_t *plug;
	plug = plugin_find( core, "vcurl");
	assert(plug);
	
	private->logger = ipc_register(core, "logger");
	plug->data = (void *) private;
	plug->start  = ipc_start;
	plug->ipc->priv = private;
	plug->ipc->cb = issue_curl;
}

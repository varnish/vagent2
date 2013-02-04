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

/*
 * Mostly just a demo/test plugin. Read for profit.
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

enum reg_status_t {
	INIT = 0,
	OK = 1,
	NOK= 2
};

struct vac_register_priv_t {
	int logger;
	enum reg_status_t status;
	int http_response;
	char* response_mesg;
	//vac specific stuff
	char* vac_url;
};

static char* curl_vac(char* vac_url) {
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init(); 	
	if(curl) {
    		curl_easy_setopt(curl, CURLOPT_URL, vac_url);
    		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
 
		/* Perform the request, res will get the return code */ 
    		res = curl_easy_perform(curl);
		/* Check for errors */ 
    		if(res != CURLE_OK) {
			return strdup("Bugger that.");
		}
		/* always cleanup */ 
		curl_easy_cleanup(curl);
		return strdup("Happy days.");
	} 
	return strdup("Bad form mate.");
}

static unsigned int vac_register_reply(struct httpd_request *request, void *data) {
	//reply callback for the vac_register module to the vac_register module
	struct vac_register_priv_t *vdata = (struct vac_register_priv_t *)data;
	logger( vdata->logger, "Registering with the VAC.");
	logger( vdata->logger, "Request type is: %d.", request->method);
	//dont forget to free the char* before reassigning it

	//curl it back to the vac
	
	free(vdata->response_mesg) ;
	vdata->response_mesg = curl_vac( vdata->vac_url);
	send_response(request->connection, vdata->http_response, vdata->response_mesg, strlen(vdata->response_mesg) );
	return 0;	
}

static void *vac_register( void* data) {
	struct agent_core_t *core = (struct agent_core_t *)data;	
	struct vac_register_priv_t *private;
	struct agent_plugin_t *plug;
	plug = plugin_find(core,"vac_register");
	assert(plug);
	private = plug->data;
	
	//make the curl call
	char* response = curl_vac( private->vac_url) ;
	free( private->response_mesg);
	private->response_mesg = response;

	logger( private->logger, "Response received from curl: %s.", private->response_mesg) ;
	return NULL;
}

static pthread_t *vac_register_start(struct agent_core_t *core, const char *name)
{
	(void)name;
	pthread_t *thread = malloc(sizeof (pthread_t));
	pthread_create(thread,NULL,(*vac_register),core);
	return thread;
}

void vac_register_init( struct agent_core_t *core) {
	struct vac_register_priv_t *private  =  malloc( sizeof(struct vac_register_priv_t) ) ;
	struct agent_plugin_t *plug;
	plug = plugin_find( core, "vac_register");
	assert(plug);

	//initialise the private data structure
	private->logger = ipc_register(core, "logger");
	private->status = INIT;
	private->http_response = 200;
	char* init_message = strdup("Happy bunnies.");
	private->response_mesg = init_message;
	private->vac_url = strdup("http://localhost:8080/api/rest/cache/addCache?ip=172.16.108.128&port=6082&cliPort=6083&secret=test123&agentId=VAGENT2");

	//chuck the private ds to the plugin so it lives on
	plug->data = (void *) private;

	//no need to kick it off just yet
	plug->start = vac_register_start; 
	
	//httpd register
	httpd_register_url(core, "/vac_register", M_GET, vac_register_reply, private);
}

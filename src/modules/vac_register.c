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

struct vac_register_priv_t {
	int logger;
	int vcurl;
	//vac specific stuff
	char* vac_url;
};

static struct ipc_ret_t *send_vcurl( struct vac_register_priv_t *private) {
	struct ipc_ret_t *vret = malloc( sizeof(struct ipc_ret_t ) );
	logger( private->logger, "registering with the vac: %s", private->vac_url);
	ipc_run(private->vcurl, vret, "%s", private->vac_url);
	return vret;	
}
 

static unsigned int vac_register_reply(struct httpd_request *request, void *data) {
	//reply callback for the vac_register module to the vac_register module
	struct vac_register_priv_t *vdata = (struct vac_register_priv_t *)data;
	struct ipc_ret_t *vret = send_vcurl( vdata);
	logger( vdata->logger, "vcurl response: status=%d answer=%s", vret->status, vret->answer); 
	send_response(request->connection, vret->status, vret->answer, strlen(vret->answer) );
	free(vret);
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
	struct ipc_ret_t *vret = send_vcurl( private); 
	logger( private->logger, "Response received from curl: status=%d answer=%s", vret->status, vret->answer);
	free(vret);
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
	private->vcurl = ipc_register(core, "vcurl");
	
	/**
	 * XXX: construct the URL based on varnish name, cli setup and vagent's own api location.
         *	pending vac api changes.
	 *	
	 *	T_arg or T_arg_orig resides in core-config->T_arg for example. name is n_arg 
         */
	private->vac_url = core->config->vac_arg;
	//chuck the private ds to the plugin so it lives on
	plug->data = (void *) private;

	//no need to kick it off just yet
	plug->start = vac_register_start; 
	
	//httpd register
	httpd_register_url(core, "/vac_register", M_GET, vac_register_reply, private);
}

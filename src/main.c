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

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "common.h"
#include "plugins.h"

static void core_opt(struct agent_core_t *core, int argc, char **argv)
{
	int opt;
	assert(core->config != NULL);
	core->config->n_arg = NULL;
	core->config->S_arg = NULL;
	core->config->T_arg = NULL;
	core->config->c_arg = "6085";
	core->config->timeout = 5;
	while ((opt = getopt(argc, argv, "n:S:T:t:c:")) != -1) {
		switch (opt) {
		case 'n':
			core->config->n_arg = optarg;
			break;
		case 'S':
			core->config->S_arg = optarg;
			break;
		case 'T':
			core->config->T_arg = optarg;
			break;
		case 't':
			core->config->timeout = strtod(optarg, NULL);
			break;
		case 'c':
			core->config->c_arg = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;
}

static void core_alloc_plugins(struct agent_core_t *core)
{
	plugin_alloc("pingd",core);
	plugin_alloc("logd",core);
	plugin_alloc("vadmin",core);
	plugin_alloc("httpd",core);
	plugin_alloc("echo",core);
	plugin_alloc("status",core);
	plugin_alloc("vcl",core);
	plugin_alloc("html",core);
	plugin_alloc("params",core);
}

static int core_plugins(struct agent_core_t *core)
{
	pingd_init(core);
	logd_init(core);
	vadmin_init(core);
	httpd_init(core);
	echo_init(core);
	status_init(core);
	vcl_init(core);
	html_init(core);
	params_init(core);
	return 1;
}

int main(int argc, char **argv)
{
	struct agent_core_t core;
	struct agent_plugin_t *plug;
	core.config = calloc(1,sizeof(struct agent_config_t));
	assert(core.config);
	core.plugins = NULL;
	core_alloc_plugins(&core);
	core_opt(&core, argc, argv);
	core_plugins(&core);
	for (plug = core.plugins; plug != NULL; plug = plug->next) {
		printf("Starting %s\n", plug->name);
		if (plug->start != NULL)
			plug->start(&core, plug->name);
	}

	for (plug = core.plugins; plug; plug = plug->next) {
		if (plug->thread) {
			printf("Waiting for %s\n", plug->name);
			pthread_join(*plug->thread, NULL);
		} else {
			printf("Plugin %s has no threads\n", plug->name);
		}
	}
	return 0;
}

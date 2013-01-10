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

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "common.h"
#include "plugins.h"

static void usage(const char *argv0)
{
	fprintf(stderr,
	"Varnish Agent usage: \n"
	"%s [-p directory] [-H directory] [-n name] [-S file]\n"
	"   [-T host:port] [-t timeout] [-c port] [-h]\n\n"
	"-p directory        Persistence directory: where VCL and parameters\n"
	"                    are stored. Default: " AGENT_PERSIST_DIR "\n"
	"-H                  Where /html/ is located. Default: " AGENT_HTML_DIR "\n"
	"-n name             Name. Should match varnishd -n option.\n"
	"-S secretfile       location of the varnishd secret file.\n"
	"-T host:port        Varnishd administrative interface.\n"
	"-t timeout          timeout for talking to varnishd.\n"
	"-c port             TCP port (default: 6085).\n"
	"-h                  Prints this.\n"
	"\n"
	"All arguments are optional.\n"
	, argv0);
}

static void core_opt(struct agent_core_t *core, int argc, char **argv)
{
	int opt;
	const char *argv0 = argv[0];
	assert(core->config != NULL);
	core->config->n_arg = NULL;
	core->config->S_arg = NULL;
	core->config->T_arg = NULL;
	core->config->c_arg = strdup("6085");
	core->config->timeout = 5;
	core->config->p_arg = strdup(AGENT_PERSIST_DIR);
	core->config->H_arg = strdup(AGENT_HTML_DIR);
	while ((opt = getopt(argc, argv, "hp:H:n:S:T:t:c:")) != -1) {
		switch (opt) {
		case 'p':
			core->config->p_arg = optarg;
			break;
		case 'H':
			core->config->H_arg = optarg;
			break;
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
		case 'h':
			usage(argv0);
			exit(1);
			break;
		}
	}

	argc -= optind;
	argv += optind;
}

/*
 * Allocate the plugin data structures for all plugins. Has to happen
 * before they are initialized, as this also include the generic IPC.
 * Otherwise ipc_register() would fail miserably.
 */
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
	plugin_alloc("ban", core);
	plugin_alloc("varnishstat",core);
	plugin_alloc("vlog", core);
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
	ban_init(core);
	varnishstat_init(core);
	vlog_init(core);
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
	/*
	 * XXX: A couple of modules use options here (vlog,vadmin,httpd)
	 * Some order is unfortunately required....
	 */
	core_opt(&core, argc, argv);
	core_plugins(&core);
	printf("Starting plugins: ");
	for (plug = core.plugins; plug != NULL; plug = plug->next) {
		printf("%s ", plug->name);
		if (plug->start != NULL)
			plug->start(&core, plug->name);
	}
	printf("\n");
	for (plug = core.plugins; plug; plug = plug->next) {
		if (plug->thread) {
			pthread_join(*plug->thread, NULL);
		}
	}
	return 0;
}

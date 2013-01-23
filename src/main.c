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

#ifdef __APPLE__
/*
 * I HATE APPLE. YOU HATE APPLE. WE ALL HATE APPLE.
 *
 * Also: daemon() is seemingly "deprecated" on apple because blah blah
 * blah, it wasn't written and approved by a guy with a turtleneck.
 * (you're supposed to use launch(d?), which obviously is about as portable
 * as anything prefixed with win32).
 *
 * So here's the deal: we define a fake macro for it, then manually define
 * the prototype ourself. Oh joy.
 */
#define daemon I_hate_you_so_much_right_now
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "config.h"
#include "pidfile.h"
#include <err.h>
#include <errno.h>

#include "common.h"
#include "plugins.h"

#ifdef __APPLE__
#undef daemon
/*
 * Pray that the gods don't change this.
 */
extern int daemon(int, int);
#endif
static void usage(const char *argv0)
{
	fprintf(stderr,
	"Varnish Agent usage: \n"
	"%s [-p directory] [-H directory] [-n name] [-S file]\n"
	"   [-T host:port] [-t timeout] [-c port] [-h] [-d]\n\n"
	"-p directory        Persistence directory: where VCL and parameters\n"
	"                    are stored. Default: " AGENT_PERSIST_DIR "\n"
	"-H                  Where /html/ is located. Default: " AGENT_HTML_DIR "\n"
	"-n name             Name. Should match varnishd -n option.\n"
	"-S secretfile       location of the varnishd secret file.\n"
	"-T host:port        Varnishd administrative interface.\n"
	"-t timeout          timeout for talking to varnishd.\n"
	"-c port             TCP port (default: 6085).\n"
	"-d                  Debug. Runs in foreground.\n"
	"-P pidfile          Write pidfile.\n"
	"-V                  Print version.\n"
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
	core->config->d_arg = 0;
	core->config->p_arg = strdup(AGENT_PERSIST_DIR);
	core->config->H_arg = strdup(AGENT_HTML_DIR);
	core->config->P_arg = NULL;
	while ((opt = getopt(argc, argv, "VhdP:p:H:n:S:T:t:c:")) != -1) {
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
		case 'd':
			core->config->d_arg = 1;
			break;
		case 'P':
			core->config->P_arg = optarg;
			break;
		case 'h':
			usage(argv0);
			exit(1);
		case 'V':
			fprintf(stderr, PACKAGE_STRING "\nCopyright (c) 2012-2013 Varnish Software AS\n");
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
	plugin_alloc("vping",core);
	plugin_alloc("logger",core);
	plugin_alloc("vadmin",core);
	plugin_alloc("httpd",core);
	plugin_alloc("echo",core);
	plugin_alloc("status",core);
	plugin_alloc("vcl",core);
	plugin_alloc("html",core);
	plugin_alloc("vparams",core);
	plugin_alloc("vban", core);
	plugin_alloc("vstat",core);
	plugin_alloc("vlog", core);
}

static int core_plugins(struct agent_core_t *core)
{
	vping_init(core);
	logger_init(core);
	vadmin_init(core);
	httpd_init(core);
	echo_init(core);
	status_init(core);
	vcl_init(core);
	html_init(core);
	vparams_init(core);
	vban_init(core);
	vstat_init(core);
	vlog_init(core);
	return 1;
}

static void p_open(struct pidfh **pfh, const char *p)
{
	pid_t otherpid;
	*pfh = pidfile_open(p, 0600, &otherpid);
	if (*pfh == NULL) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.",
			     (intmax_t)otherpid);
		}
		/* If we cannot create pidfile from other reasons, only warn. */
		warn("Cannot open or create pidfile");
	}
}

static void v_daemon(struct pidfh **pfh) 
{
	int ret;
	printf("Plugins initialized. Forking.\n");
#ifdef __APPLE__
	printf("Daemonizing is not guaranteed to not kill kittens on Mac OS X.\n");
	printf("Consider using -d to run in foreground instead.\n");
#endif
	ret = daemon(0,0);
	if (ret == -1) {
		warn("Cannot daemonize");
		if (*pfh)
			pidfile_remove(*pfh);
		exit(EXIT_FAILURE);
	}
	assert(ret == 0);
}

int main(int argc, char **argv)
{
	struct agent_core_t core;
	struct agent_plugin_t *plug;
	struct pidfh *pfh = NULL;

	core.config = calloc(1,sizeof(struct agent_config_t));
	assert(core.config);
	core.plugins = NULL;
	core_alloc_plugins(&core);
	core_opt(&core, argc, argv);
	core_plugins(&core);
	
	if (core.config->P_arg)
		p_open(&pfh, core.config->P_arg);

	if (core.config->d_arg)
		printf("Plugins initialized. -d argument given, so not forking.\n");
	else
		v_daemon(&pfh);
	
	if (pfh)
		pidfile_write(pfh);
	
	for (plug = core.plugins; plug != NULL; plug = plug->next) {
		if (plug->start != NULL)
			plug->start(&core, plug->name);
	}
	for (plug = core.plugins; plug; plug = plug->next) {
		if (plug->thread) {
			pthread_join(*plug->thread, NULL);
		}
	}
	/*
	 * XXX: Might want to do this on SIGTERM too I suppose.
	 */
	if (pfh)
		pidfile_remove(pfh);
	return 0;
}

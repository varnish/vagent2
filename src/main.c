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

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "pidfile.h"
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "base64.h"

#ifdef __APPLE__
#undef daemon
/*
 * Pray that the gods don't change this.
 */
extern int daemon(int, int);
#endif

int threads_started = 0;

static char *get_line(const char *filename)
{
	FILE *fp;
	char buffer[1024];
	unsigned int s;
	char *tmp;
	fp = fopen(filename, "r");
	if (!fp) {
		warn("Cannot open file %s for reading", filename);
		return NULL;
	}

	assert(fp);

	s = fread(buffer, 1, sizeof(buffer), fp);
	assert(s>0);
	buffer[s+1] = '\0';
	tmp = strchr(buffer,'\n');
	if (tmp)
		*tmp = '\0';
	fclose(fp);
	return strdup(buffer);
}

static void usage(const char *argv0)
{
	fprintf(stderr,
	"Varnish Agent usage: \n"
	"%s [-p directory] [-H directory] [-n name] [-S file]\n"
	"   [-T host:port] [-t timeout] [-c port] [-h] [-d]\n"
	"   [-z http://host:port] [-K agentsecretfile]\n\n"
	"-p directory          Persistence directory: where VCL and parameters\n"
	"                      are stored. Default: " AGENT_PERSIST_DIR "\n"
	"-H                    Where /html/ is located. Default: " AGENT_HTML_DIR "\n"
	"-n name               Name. Should match varnishd -n option.\n"
	"-S secretfile         location of the varnishd secret file.\n"
	"-T host:port          Varnishd administrative interface.\n"
	"-t timeout            timeout for talking to varnishd.\n"
	"-c port               TCP port (default: 6085).\n"
	"-d                    Debug. Runs in foreground.\n"
	"-P pidfile            Write pidfile.\n"
	"-V                    Print version.\n"
	"-h                    Prints this.\n"
	"-u user               User to run as(default: varnish)\n"
	"-g group              Group to run as (default: varnish)\n"
	"-q                    Quiet mode. Only log/output warnings and errors\n"
	"-v                    Verbose mode. Output everything.\n"
	"-K agentsecretfile    File containing username:password for authentication\n"
	"-z http://host:port   VAC interface.\n"
	"-a host               The host name the VAC should call back.\n"
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
	core->config->S_arg_fd = -1;
	core->config->T_arg = NULL;
	core->config->g_arg = NULL;
	core->config->u_arg = NULL;
	core->config->T_arg_orig = NULL;
	core->config->c_arg = strdup("6085");
	core->config->timeout = 5;
	core->config->d_arg = 0;
	core->config->p_arg = strdup(AGENT_PERSIST_DIR);
	core->config->H_arg = strdup(AGENT_HTML_DIR);
	core->config->P_arg = NULL;
	core->config->vac_arg= NULL;
	core->config->a_arg = NULL;
	core->config->K_arg = strdup("/etc/varnish/agent_secret");
	core->config->loglevel = 2;
	while ((opt = getopt(argc, argv, "VhdP:p:H:n:S:T:t:c:u:g:z:a:K:qv")) != -1) {
		switch (opt) {
		case 'q':
			core->config->loglevel = 1;
			break;
		case 'v':
			core->config->loglevel = 3;
			break;
		case 'K':
			core->config->K_arg = optarg;
			break;
		case 'p':
			core->config->p_arg = optarg;
			break;
		case 'u':
			core->config->u_arg = optarg;
			break;
		case 'g':
			core->config->g_arg = optarg;
			break;
		case 'H':
			core->config->H_arg = optarg;
			break;
		case 'n':
			core->config->n_arg = optarg;
			break;
		case 'S':
			// avoid fd leak
			if (core->config->S_arg_fd > 0)
				close(core->config->S_arg_fd);
			core->config->S_arg = optarg;
			core->config->S_arg_fd = open(optarg, O_RDONLY);
			break;
		case 'T':
			core->config->T_arg_orig = optarg;
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
		case 'z':
			core->config->vac_arg = strdup(optarg);
			break;
		case 'a':
			core->config->a_arg = optarg;
			break;
		}
	}

	assert(core->config->K_arg);	
	core->config->userpass = get_line(core->config->K_arg);
	if (!core->config->userpass) {
		errx(1,"No password present. Put one in %s using \"user:password\" format", core->config->K_arg);
	}
	core->config->user = strdup(core->config->userpass);
	core->config->password = strchr(core->config->user, ':');
	if (core->config->password == NULL || *(core->config->password + 1)== '\0')
		errx(1, "Username and password string does not contain a colon. Format: \"username:password\"");
	*core->config->password = '\0';
	core->config->password++;


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
#define PLUGIN(plug) \
	plugin_alloc(#plug, core);
#include "plugin-list.h"
#undef PLUGIN
}

static int core_plugins(struct agent_core_t *core)
{
#define PLUGIN(plug) \
	plug ## _init(core);
#include "plugin-list.h"
#undef PLUGIN
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
#ifdef __APPLE__
	logger(-1, "Daemonizing is not guaranteed to not kill kittens on Mac OS X.\n");
	logger(-1, "Consider using -d to run in foreground instead.\n");
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

/*
 * Change user/group if we can.
 */
static void sandbox(struct agent_core_t *core)
{
	int ret;
	struct passwd *pw;
	if (geteuid() == 0) {
		pw = getpwnam(core->config->u_arg ? core->config->u_arg : "varnish");
		if (pw == NULL) {
			errx(1,"%s is not a valid user\n", core->config->u_arg ? core->config->u_arg: "varnish");
		}

		if (!core->config->g_arg) {
			ret = setgid(pw->pw_gid);
			assert(ret == 0);
		} else {
			struct group *gr;
			gr = getgrnam(core->config->g_arg ? core->config->g_arg : "varnish");
			if (gr == NULL) {
				errx(1,"%s is not a valid group\n", core->config->g_arg ? core->config->g_arg : "varnish");
			}
			ret = setgid(gr->gr_gid);
			assert(ret == 0);
		}
		ret = setuid(pw->pw_uid);
		assert(ret == 0);
	} else {
		logger(-1, "Not running as root, no priv-sep");
	}
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
	base64_init();
	core_plugins(&core);
	
	if (core.config->P_arg)
		p_open(&pfh, core.config->P_arg);

	sandbox(&core);
	if (core.config->d_arg)
		logger(-1, "Plugins initialized. -d argument given, so not forking.");
	else
		v_daemon(&pfh);
		
	if (pfh)
		pidfile_write(pfh);
	ipc_sanity(&core);
	threads_started = 1;
	for (plug = core.plugins; plug != NULL; plug = plug->next) {
		if (plug->start != NULL)
			plug->thread = plug->start(&core, plug->name);
	}
	threads_started = 2;
	for (plug = core.plugins; plug; plug = plug->next) {
		if (plug->thread) {
			pthread_join(*(pthread_t *)plug->thread, NULL);
			free(plug->thread);
		}
	}
	/*
	 * XXX: Might want to do this on SIGTERM too I suppose.
	 */
	if (pfh)
		pidfile_remove(pfh);
	return 0;
}

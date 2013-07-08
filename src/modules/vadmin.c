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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <varnishapi.h>
#include <vcli.h>

#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "vss-hack.h"
#include "http.h"
#include "helpers.h"

struct vadmin_config_t {
	int sock;
	int state;
	int logger;
	int s_arg_fd;
};

static int
cli_write(int sock, const char *s)
{
	int i, l;

	i = strlen(s);
	l = write(sock, s, i);
	if (i == l)
		return 1;
	perror("Write error CLI socket");
	close(sock);
	return 0;
}

/*
 * Parse the -n argument and populate -T if necessary.
 *
 * Run on every connect, must do some cleanup.
 *
 */
static int
n_arg_sock(struct agent_core_t *core)
{
	struct VSM_data *vsd;
	struct vadmin_config_t *vadmin;
	char *p;
	GET_PRIV(core, vadmin);

	vsd = VSM_New();
	assert(VSL_Arg(vsd, 'n', core->config->n_arg));
	if (VSM_Open(vsd, 1)) {
		warnlog(vadmin->logger,"Couldn't open VSM");
		VSM_Delete(vsd);
		vsd = NULL;
	}

	if (core->config->T_arg_orig) {
		if (core->config->T_arg)
			free(core->config->T_arg);
		core->config->T_arg = strdup(core->config->T_arg_orig);
	} else {
		if (vsd == NULL) {
			warnlog(vadmin->logger,"No -T arg and no shmlog readable.");
			return -1;
		}
		p = VSM_Find_Chunk(vsd, "Arg", "-T", "", NULL);
		if (p == NULL)  {
			warnlog(vadmin->logger, "No -T arg in shared memory.");
			return (-1);
		}
		core->config->T_arg = strdup(p);
	}

	if (core->config->S_arg == NULL && !vsd) {
		warnlog(vadmin->logger, "No shmlog and no -S arg. Unknown if authentication will work.");
	}
	if (vsd && core->config->S_arg == NULL) {
		p = VSM_Find_Chunk(vsd, "Arg", "-S", "", NULL);
		if (p != NULL)
			core->config->S_arg = strdup(p);
	}

	p = strchr(core->config->T_arg, '\n');
	if (p) {
		assert(p);
		*p = '\0';
	}
	if (vsd)
		VSM_Delete(vsd);
	logger(vadmin->logger, "-T argument computed to: %s", core->config->T_arg ? core->config->T_arg : "(null)");
	return (1);
}

/*
 * This function establishes a connection to the specified ip and port and
 * sends a command to varnishd. If varnishd returns an OK status, the result
 * is printed and 0 returned. Else, an error message is printed and 1 is
 * returned
 */
static int
cli_sock(struct vadmin_config_t *vadmin, struct agent_core_t *core)
{
	unsigned status;
	char *answer = NULL;
	char buf[CLI_AUTH_RESPONSE_LEN + 1];
	n_arg_sock(core);
	if (core->config->T_arg == NULL) {
		warnlog(vadmin->logger, "No T-arg (Administration port) available. Varnishadm-commands wont work.");
		return (-1);
	}
	vadmin->sock = VSS_open(vadmin->logger, core->config->T_arg, core->config->timeout);
	if (vadmin->sock < 0) {
		warnlog(vadmin->logger, "Connection failed (%s)", core->config->T_arg);
		return (-1);
	}

	(void)VCLI_ReadResult(vadmin->sock, &status, &answer, core->config->timeout);
	if (status == CLIS_AUTH) {
		if (core->config->S_arg == NULL) {
			warnlog(vadmin->logger, "Authentication required and no -S arg found");
			assert(close(vadmin->sock) == 0);
			return(-1);
		}
		if (vadmin->s_arg_fd == -1) {
			vadmin->s_arg_fd = open(core->config->S_arg, O_RDONLY);
		} else {
			lseek(vadmin->s_arg_fd, 0, SEEK_SET);
		}
		if (vadmin->s_arg_fd < 0) {
			warnlog(vadmin->logger, "Cannot open \"%s\": %s",
			    core->config->S_arg, strerror(errno));
			assert(close(vadmin->sock) == 0);
			return (-1);
		}
		VCLI_AuthResponse(vadmin->s_arg_fd, answer, buf);
		free(answer);

		cli_write(vadmin->sock, "auth ");
		cli_write(vadmin->sock, buf);
		cli_write(vadmin->sock, "\n");
		(void)VCLI_ReadResult(vadmin->sock, &status, &answer, core->config->timeout);
		if (status != CLIS_OK) {
			warnlog(vadmin->logger, "Failed authentication.");
			assert(close(vadmin->s_arg_fd) == 0);
			vadmin->s_arg_fd = -1;
		}

	}
	if (status != CLIS_OK) {
		warnlog(vadmin->logger, "Rejected %u\n%s", status, answer);
		assert(close(vadmin->sock) == 0);
		return (-1);
	}
	free(answer);

	cli_write(vadmin->sock, "ping\n");
	(void)VCLI_ReadResult(vadmin->sock, &status, &answer, core->config->timeout);
	if (status != CLIS_OK || strstr(answer, "PONG") == NULL) {
		warnlog(vadmin->logger, "No pong received from server");
		assert(close(vadmin->sock) == 0);
		return(-1);
	}
	free(answer);
	vadmin->state = 1;

	return (vadmin->sock);
}

static void
vadmin_run(struct vadmin_config_t *vadmin, char *cmd, struct ipc_ret_t *ret)
{
	int sock = vadmin->sock;
	int nret;
	assert(cmd);
	nret = cli_write(sock, cmd);
	if (!nret) {
		warnlog(vadmin->logger, "Communication error with varnishd.");
		ret->status = 400;
		ret->answer = strdup("Varnishd disconnected");
		vadmin->state = 0;
		return;
	}
	nret = cli_write(sock, "\n");
	if (!nret) {
		warnlog(vadmin->logger, "Communication error with varnishd.");
		ret->status = 400;
		ret->answer = strdup("Varnishd disconnected");
		vadmin->state = 0;
		return;
	}
	debuglog(vadmin->logger, "Running %s",cmd);
	(void)VCLI_ReadResult(sock, &ret->status, &ret->answer, 2000);
	debuglog(vadmin->logger, "Got: %d ",ret->status);
}

static void
read_cmd(void *private, char *msg, struct ipc_ret_t *ret)
{
	struct agent_core_t *core = private;
	struct agent_plugin_t *plug;
	struct vadmin_config_t *vadmin;
	plug = plugin_find(core,"vadmin");
	vadmin = plug->data;
	if (vadmin->state == 0)
		cli_sock(vadmin, core);
	vadmin_run(vadmin, msg, ret);
}


void
vadmin_init(struct agent_core_t *core)
{
	struct vadmin_config_t *vadmin;
	struct agent_plugin_t *v;

	v  = plugin_find(core, "vadmin");
	v->ipc->cb = read_cmd;
	vadmin = malloc(sizeof(struct vadmin_config_t));
	v->data = vadmin;
	v->ipc->priv = core;
	v->start = ipc_start;
	vadmin->sock = -1;
	vadmin->state = 0;
	vadmin->s_arg_fd = -1;
	vadmin->logger = ipc_register(core, "logger");

	if (core->config->n_arg != NULL) {
		if (core->config->T_arg != NULL || core->config->S_arg != NULL) {
			return ;
		}
	} else if (core->config->T_arg == NULL) {
		core->config->n_arg = strdup("");
	} else {
		assert(core->config->T_arg != NULL);
	}
	signal(SIGPIPE, SIG_IGN);
	if (vadmin->state == 0)
		cli_sock(vadmin, core);
	return ;
}

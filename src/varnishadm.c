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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <varnishapi.h>
#include "vcli.h"

#include "common.h"
#include "plugins.h"
#include "ipc.h"

#define RL_EXIT(s) exit(s)

struct vadmin_config_t {
	int sock;
	int logger;
};

static int
cli_write(int sock, const char *s)
{
	int i, l;

	i = strlen(s);
	l = write (sock, s, i);
	if (i == l)
		return 1;
	perror("Write error CLI socket");
	assert(close(sock) == 0);
	return 0;
}

int
VSS_open(const char *str, double tmo);
/*
 * This function establishes a connection to the specified ip and port and
 * sends a command to varnishd. If varnishd returns an OK status, the result
 * is printed and 0 returned. Else, an error message is printed and 1 is
 * returned
 */
static int
cli_sock(struct vadmin_config_t *vadmin, struct agent_core_t *core)
{
	int fd;
	unsigned status;
	char *answer = NULL;
	char buf[CLI_AUTH_RESPONSE_LEN + 1];
	printf("Timeout: %f\n", core->config->timeout);	
	vadmin->sock = VSS_open(core->config->T_arg, core->config->timeout);
	if (vadmin->sock < 0) {
		fprintf(stderr, "Connection failed (%s)\n", core->config->T_arg);
		return (-1);
	}

	(void)VCLI_ReadResult(vadmin->sock, &status, &answer, core->config->timeout);
	if (status == CLIS_AUTH) {
		if (core->config->S_arg == NULL) {
			fprintf(stderr, "Authentication required\n");
			assert(close(vadmin->sock) == 0);
			return(-1);
		}
		fd = open(core->config->S_arg, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "Cannot open \"%s\": %s\n",
			    core->config->S_arg, strerror(errno));
			assert(close(vadmin->sock) == 0);
			return (-1);
		}
		VCLI_AuthResponse(fd, answer, buf);
		assert(close(fd) == 0);
		free(answer);

		cli_write(vadmin->sock, "auth ");
		cli_write(vadmin->sock, buf);
		cli_write(vadmin->sock, "\n");
		(void)VCLI_ReadResult(vadmin->sock, &status, &answer, core->config->timeout);
	}
	if (status != CLIS_OK) {
		fprintf(stderr, "Rejected %u\n%s\n", status, answer);
		assert(close(vadmin->sock) == 0);
		return (-1);
	}
	free(answer);

	cli_write(vadmin->sock, "ping\n");
	(void)VCLI_ReadResult(vadmin->sock, &status, &answer, core->config->timeout);
	if (status != CLIS_OK || strstr(answer, "PONG") == NULL) {
		fprintf(stderr, "No pong received from server\n");
		assert(close(vadmin->sock) == 0);
		return(-1);
	}
	free(answer);

	return (vadmin->sock);
}

void
vadmin_run(struct vadmin_config_t *vadmin, char *cmd, struct ipc_ret_t *ret)
{
	int sock = vadmin->sock;
	assert(cmd);
	cli_write(sock, cmd);
	cli_write(sock, "\n");
	logger(vadmin->logger, "Running %s",cmd);
	(void)VCLI_ReadResult(sock, &ret->status, &ret->answer, 2000);
	logger(vadmin->logger, "Got: %d ",ret->status, ret->answer);
}

void
read_cmd(void *private, char *msg, struct ipc_ret_t *ret)
{
	struct vadmin_config_t *vadmin = (struct vadmin_config_t *) private;
	vadmin_run(vadmin, msg, ret);
}

static int
n_arg_sock(struct agent_core_t *core)
{
	struct VSM_data *vsd;
	char *p;

	vsd = VSM_New();
	assert(VSL_Arg(vsd, 'n', core->config->n_arg));
	if (VSM_Open(vsd, 1)) {
		fprintf(stderr, "Couldn't open VSM\n");
		return (-1);
	}

        if (core->config->T_arg == NULL) {
                p = VSM_Find_Chunk(vsd, "Arg", "-T", "", NULL);
                if (p == NULL)  {
                        fprintf(stderr, "No -T arg in shared memory\n");
                        return (-1);
                }
		core->config->T_arg = strdup(p);
        }
        if (core->config->S_arg == NULL) {
		p = VSM_Find_Chunk(vsd, "Arg", "-S", "", NULL);
		if (p != NULL)
			core->config->S_arg = strdup(p);
	}

	p = strchr(core->config->T_arg, '\n');
	assert(p);
	*p = '\0';
	return (1);
}

void
vadmin_init(struct agent_core_t *core)
{
	struct vadmin_config_t *vadmin;
	struct agent_plugin_t *v;

	v  = plugin_find(core, "vadmin");
	v->ipc->cb = read_cmd;
	vadmin = malloc(sizeof(struct vadmin_config_t ));
	v->data = vadmin;
	v->ipc->priv = vadmin;
	v->start = ipc_start;
	vadmin->sock = -1;

	if (core->config->n_arg != NULL) {
		if (core->config->T_arg != NULL || core->config->S_arg != NULL) {
			return ;
		}
		n_arg_sock(core);
	} else if (core->config->T_arg == NULL) {
		core->config->n_arg = "";
		n_arg_sock(core);
	} else {
		assert(core->config->T_arg != NULL);
	}
	cli_sock(vadmin, core);
	vadmin->logger = ipc_register(core, "logd");
	if (vadmin->sock < 0)
		exit(2);
		
	return ;
}

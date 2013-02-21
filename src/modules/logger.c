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

#define _GNU_SOURCE

#include "common.h"
#include "plugins.h"
#include "ipc.h"

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <syslog.h>

/*
 * TODO:
 *  - Write to something slightly more elaborate than stdout.
 */
struct logger_priv_t {
	int debug;
	int loglevel;
};

/*
 * XXX: Needed for handling assert() failures.
 */
static int logger_open=0;

static void read_log(void *private, char *msg, struct ipc_ret_t *ret)
{
	struct logger_priv_t *log = (struct logger_priv_t *) private;
	int loglevel;
	assert(log);
	loglevel = msg[0] - '0';
	ret->status = 200;
	ret->answer = strdup("OK");
	if (loglevel > log->loglevel)
		return ;
	msg++;

	if (log->debug)
		printf("LOGGER: %s\n",msg);
	else
		syslog(LOG_INFO,"%s",msg);
}

void assert_fail(const char *expr, const char *file, int line, const char *func)
{
	char *string;
	int ret;
	ret = asprintf(&string, "%s:%d: %s: Assertion `%s' failed. Aborting.", file, line, func, expr);
	if (ret < 10) {
		fprintf(stderr, "Assert, then asprintf failure. ABANDON SHIP.\n");
		abort();
	}
	if (logger_open) {
		syslog(LOG_CRIT, "%s", string);
		closelog();
	} else
		fprintf(stderr,"%s\n",string);
	abort();
}

void logger_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct logger_priv_t *priv = malloc(sizeof(struct logger_priv_t));
	plug = plugin_find(core,"logger");

	priv->loglevel = core->config->loglevel;
	if (core->config->d_arg) {
		priv->debug = 1;
		logger_open = 0;
	} else {
		priv->debug = 0;
		openlog("varnish-agent",LOG_PID,LOG_DAEMON);
		logger_open = 1;
	}
	plug->ipc->cb = read_log;
	plug->ipc->priv = priv;
	plug->data = (void *)priv;
	plug->start = ipc_start;
}

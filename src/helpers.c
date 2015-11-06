/*
 * Copyright (c) 2013 Varnish Software AS
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

/*
 * Misc common helper functions
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <microhttpd.h>

#include "common.h"
#include "http.h"
#include "helpers.h"
#include "ipc.h"


void
run_and_respond_eok(int vadmin, struct MHD_Connection *conn,
    unsigned min, unsigned max, const char *fmt, ...)
{
	struct ipc_ret_t vret;
	va_list ap;
	char *buffer;
	int iret;

	va_start(ap, fmt);
	iret = vasprintf(&buffer, fmt, ap);
	assert(iret>0);
	va_end(ap);
	ipc_run(vadmin, &vret, "%s", buffer);
	free(buffer);

	if (vret.status >= min && vret.status <= max)
		http_reply(conn, 200, vret.answer);
	else
		http_reply(conn, 500, vret.answer);
	free(vret.answer);
}

/*
 * Run a varnishadm-command and send the result of that command back to the
 * http connection. If varnishd returns 200, then so do we. Otherwise: 500.
 */
void
run_and_respond(int vadmin, struct MHD_Connection *conn, const char *fmt, ...)
{
	struct ipc_ret_t vret;
	va_list ap;
	char *buffer;
	int iret;

	va_start(ap, fmt);
	iret = vasprintf(&buffer, fmt, ap);
	assert(iret >= 0);
	va_end(ap);
	ipc_run(vadmin, &vret, "%s", buffer);
	free(buffer);

	http_reply(conn, vret.status == 200 ? 200 : 500, vret.answer);
	free(vret.answer);
}

unsigned int
help_reply(struct http_request *request, void *data)
{
	http_reply(request->connection, 200, (char *)data);
	return 0;
}

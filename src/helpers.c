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
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <microhttpd.h>

#include "common.h"
#include "ipc.h"
#include "helpers.h"
#include "httpd.h"

/*
 * Run a varnishadm-command and send the result of that command back.
 */
void run_and_respond(int vadmin, struct MHD_Connection *conn, const char *fmt, ...)
{
	struct ipc_ret_t vret;
	va_list ap;
	char *buffer;
	int iret;


	va_start(ap, fmt);
	iret = vasprintf(&buffer, fmt, ap);
	assert(iret>0);
	va_end(ap);
	ipc_run(vadmin, &vret, buffer);
	free(buffer);

	if (vret.status == 200)
		send_response_ok(conn, vret.answer);
	else
		send_response_fail(conn, vret.answer);
	free(vret.answer);
}


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

#ifndef HELPERS_H
#define HELPERS_H

/*
 * Generic http resonder that takes a char * as data and echos it back.
 * Used for /help/ stuff.
 */
unsigned int help_reply(struct http_request *request, void *data);

/*
 * Run the command given on fmt and respond to the connection.
 */
void run_and_respond(int vadmin, struct MHD_Connection *conn, const char *fmt, ...);
void run_and_respond_eok(int vadmin, struct MHD_Connection *conn,
			 unsigned min, unsigned max, const char *fmt, ...);

/*
 * Got core, need priv?
 * struct myplugstuff *pluginname;
 * GET_PRIV(core,pluginname);
 */
#define GET_PRIV(core,plug) do { \
	struct agent_core_t *core_tmp = (struct agent_core_t *)core; \
	struct agent_plugin_t *p_tmp; \
	assert(core_tmp); \
	p_tmp = plugin_find(core_tmp,#plug); \
	plug = p_tmp->data; \
	assert(plug); \
} while(0)
	
#endif

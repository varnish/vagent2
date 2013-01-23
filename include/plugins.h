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

#ifndef PLUGINS_H
#define PLUGINS_H

#include "common.h"

/*
 * Basic plugin functions.
 *
 * All plugins are first allocated with plugin_alloc, then some
 * configuration is done, then initialization.
 */

/*
 * Search for a plugin.
 */
struct agent_plugin_t *plugin_find(struct agent_core_t *core, const char *name);

/*
 * Allocate a plugin and IPC. Does NOT init the plugin.
 */
void plugin_alloc(const char *name, struct agent_core_t *core);

/*
 * Init functions for said plugins.
 *
 * Should eventually be dealt with by dlopen etc.
 *
 * These are all run in the context of the main thread. These functions are
 * used to register ipc-interests, url-handlers, log-handlers etc.
 *
 */
void pingd_init(struct agent_core_t *core);
void logger_init(struct agent_core_t *core);
void httpd_init(struct agent_core_t *core);
void echo_init(struct agent_core_t *core);
void status_init(struct agent_core_t *core);
void vcl_init(struct agent_core_t *core);
void html_init(struct agent_core_t *core);
void vadmin_init(struct agent_core_t *core);
void params_init(struct agent_core_t *core);
void ban_init(struct agent_core_t *core);
void vstat_init(struct agent_core_t *core);
void vlog_init(struct agent_core_t *core);
#endif


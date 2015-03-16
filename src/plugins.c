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
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "ipc.h"
#include "plugins.h"

struct agent_plugin_t *plugin_find(struct agent_core_t *core, const char *name)
{
	struct agent_plugin_t *plug;
	assert(core);
	for (plug = core->plugins; plug != NULL; plug = plug->next) {
		if (!strcmp(name,plug->name))
			return plug;
	}
	/*
	 * XXX: In the future, this could be an acceptable way of detecting
	 * optional plugins, but for now, we know that all plugins ever
	 * written are always loaded, so plugin_find() should always return
	 * something
	 */
	assert("plugin_find() returning NULL");
	/* NOTREACHED */
	return (NULL);
}

/*
 * Allocate memory and IPC for a plugin. init the IPC.
 */
void plugin_alloc(const char *name, struct agent_core_t *core)
{
	struct agent_plugin_t *plug;

	ALLOC_OBJ(plug);
	ALLOC_OBJ(plug->ipc);
	ipc_init(plug->ipc);
	plug->name = strdup(name);
	assert(plug->name);
	plug->next = core->plugins;
	core->plugins = plug;
}


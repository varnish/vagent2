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

#ifndef IPC_H
#define IPC_H

/*
 * Essentially the upper bound on plugins.
 */
#define MAX_LISTENERS 1024

/*
 * Structure used to return answers to IPC calls.
 *
 * The caller must free *answer.
 */
struct ipc_ret_t {
	unsigned int status;
	char *answer;
};

/*
 * IPC structure used to track IPC listeners etc.
 * All plugins have an ipc_t-structure associated with them, even if they
 * are unused.
 *
 * Set the cb to the function that will be called when the IPC gets an
 * event. *priv will be passed to the cb.
 */
struct ipc_t {
	int listeners[MAX_LISTENERS];
	int nlisteners;
	void  (*cb)(void *priv, char *msg, struct ipc_ret_t *ret);
	void *priv;
};

/*
 * Register a listener.
 *
 * Supply the core and the name of the plugin and the register will search
 * up the correct structure and do the rest. It returns a handle that will
 * later be used with ipc_run().
 */
int ipc_register(struct agent_core_t *core, const char *name);

/*
 * Execute an IPC command. The handle is that returned from ipc_register().
 *
 * *ret will contain the reply. The ret->answer needs to be freed.
 */
void ipc_run(int handle, struct ipc_ret_t *ret, const char *fmt, ...);

/*
 * Send an IPC message.
 *
 * Same as ipc_run, but allows sending arbitrary data (e.g: Binary).
 * ipc_run() uses ipc_send() under the hood.
 */
void ipc_send(int handle, void *data, int len, struct ipc_ret_t *ret);

/*
 * Inits the ipc structure. E.g: making sure everything is 0.
 *
 * XXX: Possibly redundant.
 */
void ipc_init(struct ipc_t *ipc);

/*
 * Starts the IPC listener for the named plugin and returns the thread (if
 * any).
 */
pthread_t *ipc_start(struct agent_core_t *core, const char *name);

/*
 * Sanity function run by main() to verify that all plugins that seemingly
 * use a callback also provide a start function. Not doing so means you
 * have a broken IPC.
 */
void ipc_sanity(struct agent_core_t *core);
#endif


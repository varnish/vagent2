/*
 * Copyright (c) 2012-2015 Varnish Software Group
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

#ifndef COMMON_H
#define COMMON_H

/*
 * Configuration, handled by main for now.
 */
struct agent_config_t {
	/*
	 * Varnishadm-related:
	 */
	double timeout;
	char *T_arg;
	char *T_arg_orig;
	char *S_arg;
	int S_arg_fd;
	char *n_arg;
	char *u_arg;
	char *g_arg;
	const char *K_arg;
	int r_arg;

	int d_arg; // 0 - fork. 1 - foreground.
	int loglevel;
	const char *c_arg; // Listening port for incoming requests
	char *C_arg; // CURLOPT_CAINFO param
	const char *p_arg; // Persistence directory
	const char *H_arg; // HTML directory
	char *P_arg; // Pid file
	char *vac_arg;
	char *password;
	char *user;
	char *userpass;
};

/*
 * Core structure.
 *
 * Only contains the config list and plugins, but is used all over.
 *
 * While searching for a plugin frequently is "wasteful", the number of
 * plugins present is so few that the simplicity offered outweighs the
 * disadvantages.
 */
struct agent_core_t {
	struct agent_config_t *config;
	struct agent_plugin_t *plugins;
};

/*
 * Generic plugin structure.
 *
 * Name is the text name of the plugin, which should match the file name.
 * It is used to search for the plugin.
 * *data is a private data structure for the plugin.
 * the start-function is run to start the plugin. If the plugin only
 * listens to an IPC (e.g: varnishadm, logger, ...) this can be set to
 * ipc_start. Otherwise a custom function needs to be provided, or NULL for
 * plugins that do not run in the background (e.g: they trigger in the
 * context of other plugins, like /html/).
 * thread needs to be set to the thread, if it exists.
 *
 * XXX: Having both a return type of pthread_t on start and a
 * thread-reference here is a bit redundant...
 */
struct agent_plugin_t {
	const char *name;
	void *data;
	struct ipc_t *ipc;
	struct agent_plugin_t *next;
	void *(*start)(struct agent_core_t *core, const char *name);
	void *thread;
};

extern int threads_started;
/*
 * Logger macro to include file, func, line etc.
 * Register with the logger-plugin and use that as the handle.
 * The "l < 0" thing is allow main.c to use the logger() too.
 */

#define loggerraw(l, level, fmt, ...) do { \
	struct ipc_ret_t logger_thing_int; \
	if (threads_started == 0 || l < 0) \
		printf("%s (%s:%d): " fmt "\n" , __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
	else {\
		ipc_run(l, &logger_thing_int, #level "%s (%s:%d): " fmt , __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
		free(logger_thing_int.answer); \
	} \
} while(0)

#define logger(l,fmt,...) loggerraw(l, 2, fmt, ##__VA_ARGS__)
#define warnlog(l,fmt,...) loggerraw(l, 1, fmt, ##__VA_ARGS__)
#define debuglog(l,fmt,...) loggerraw(l, 3, fmt, ##__VA_ARGS__)

void assert_fail(const char *expr, const char *file, int line, const char *func)  __attribute__((__noreturn__));

#define assert(expr) \
  	((expr) ? (void)(0) : assert_fail(#expr, __FILE__, __LINE__, __func__));

#define ALLOC_OBJ(to)			\
do {					\
	(to) = calloc(sizeof *(to), 1);	\
	assert((to) != NULL);		\
} while(0)

#define AZ(expr)			\
do {					\
	assert((expr) == 0);		\
} while(0)

#define DUP_OBJ(to, from, len)		\
do {					\
	assert((from) != NULL);		\
	(to) = malloc((len) + 1);	\
	assert((to) != NULL);		\
	memcpy((to), (from), (len));	\
	(to)[(len)] = '\0';		\
} while(0)

#define GET_PRIV(core, plug)		\
do {					\
	struct agent_plugin_t *pp;	\
	pp = plugin_find(core, #plug);	\
	plug = pp->data;		\
	assert(plug);			\
} while(0)

#define STARTS_WITH(string, match) !strncmp(string, match, strlen(match))

#endif

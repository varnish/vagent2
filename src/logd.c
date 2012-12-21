#include "common.h"
#include "plugins.h"
#include "ipc.h"

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <poll.h>
#include <sys/socket.h>

/*
 * TODO:
 *  - Write to something slightly more elaborate than stdout.
 */
struct logd_priv_t {
	FILE *out;
	struct ipc_t *ipc;
};

void read_log(void *private, char *msg, struct ipc_ret_t *ret)
{
	struct logd_priv_t *log = (struct logd_priv_t *) private;

	fprintf(log->out,"LOGGER: %s\n",msg);
	
	ret->status = 200;
	ret->answer = "OK";
}

void logd_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct logd_priv_t *priv = malloc(sizeof(struct logd_priv_t));
	plug = plugin_find(core,"logd");
	
	priv->out = stdout;
	plug->ipc->cb = read_log;
	plug->ipc->priv = priv;
	plug->data = (void *)priv;
	plug->start = ipc_start;
}

/*
 * Underlying log writer.
 *
 * This is used by the logger() macro to send the log entry.
 */
void logger_real(int handle, const char *file, const char *func, const unsigned int line, const char *fmt, ...)
{
	va_list ap;
	struct ipc_ret_t ret;
	char buffer2[1024];

	va_start(ap, fmt);
	vsnprintf(buffer2, 1024, fmt, ap);
	va_end(ap);
	ipc_run(handle,&ret,"%s (%s:%d): %s", func, file, line, buffer2);
}

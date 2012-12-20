#ifndef IPC_H
#define IPC_H

#include "common.h"
#define MAX_LISTENERS 1024
struct ipc_ret_t {
	unsigned int status;
	char *answer;
};
struct ipc_t {
	int listeners[MAX_LISTENERS];
	unsigned int nlisteners;
	void  (*cb)(void *priv, char *msg, struct ipc_ret_t *ret);
	void *priv;
};

int ipc_register(struct agent_core_t *core, char *name) ;
void ipc_run(int handle, struct ipc_ret_t *ret, char *fmt, ...);

void ipc_init(struct ipc_t *ipc);
pthread_t *ipc_start(struct agent_core_t *core, const char *name);

#endif


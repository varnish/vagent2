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
	unsigned int nlisteners;
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
int ipc_register(struct agent_core_t *core, char *name) ;

/*
 * Execute an IPC command. The handle is that returned from ipc_register().
 *
 * *ret will contain the reply. The ret->answer needs to be freed.
 */
void ipc_run(int handle, struct ipc_ret_t *ret, char *fmt, ...);

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

#endif


#ifndef COMMON_H
#define COMMON_H

#define MAX_LISTENERS 1024
struct vadmin_ret {
	unsigned int status;
	char *answer;
};

struct vadmin_config_t {
	double timeout;
	char *T_arg;
	char *S_arg;
	char *n_arg;
	int sock;
	int listeners[MAX_LISTENERS];
	int nlisteners;
};

struct agent_plugin_t {
	const char *name;
	void *data;
	struct agent_plugin_t *next;
};

struct agent_core_t {
	struct vadmin_config_t *vadmin;
	struct agent_plugin_t *plugins;
};


#endif


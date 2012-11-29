#ifndef VARNISHADM_H
#define VARNISHADM_H


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
};


struct vadmin_config_t *
vadmin_preconf(void);

void
vadmin_run(struct vadmin_config_t *vadmin, char *cmd, struct vadmin_ret *ret);

int vadmin_init(struct vadmin_config_t *vadmin);
int vadmin_send(char *str);
#endif

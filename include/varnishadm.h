#ifndef VARNISHADM_H
#define VARNISHADM_H

int vadmin_init(int argc, char **argv);
int vadmin_send(char *str);

struct vadmin_ret {
	unsigned int status;
	char *answer;
};
void
vadmin_run(int sock, char *cmd, struct vadmin_ret *ret);

#endif

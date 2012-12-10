#ifndef VARNISHADM_H
#define VARNISHADM_H

#include "main.h"
#include "common.h"



struct vadmin_config_t *
vadmin_preconf(void);

void
vadmin_run(int fd, char *cmd, struct vadmin_ret *ret);

int
vadmin_register(struct agent_core_t *core);

int vadmin_init(struct vadmin_config_t *vadmin);
int vadmin_send(char *str);
int
vadmin_start(struct vadmin_config_t *vadmin);
#endif

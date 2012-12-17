#ifndef VARNISHADM_H
#define VARNISHADM_H
#include <pthread.h>
#include "main.h"
#include "common.h"



void vadmin_preconf(struct agent_core_t *core);

int vadmin_register(struct agent_core_t *core);
int vadmin_init(struct agent_core_t *core);
int vadmin_send(char *str);
pthread_t *vadmin_start(struct agent_core_t *core, char *name);
#endif

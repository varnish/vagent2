#ifndef VARNISHADM_H
#define VARNISHADM_H
#include <pthread.h>
#include "main.h"
#include "common.h"




int vadmin_register(struct agent_core_t *core);
int vadmin_init(struct agent_core_t *core);
int vadmin_send(char *str);
#endif

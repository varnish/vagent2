
#ifndef PLUGINS_H
#define PLUGINS_H

#include "common.h"

struct agent_plugin_t *plugin_find(struct agent_core_t *core, char *name);

void pingd_init(struct agent_core_t *core);
void pingd_start(struct agent_core_t *core);

#endif


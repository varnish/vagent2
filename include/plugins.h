
#ifndef PLUGINS_H
#define PLUGINS_H

#include "common.h"
struct agent_plugin_t *plugin_find(struct agent_core_t *core, const char *name);
void plugin_alloc(char *name, struct agent_core_t *core);

void pingd_init(struct agent_core_t *core);

void logd_init(struct agent_core_t *core);
#endif


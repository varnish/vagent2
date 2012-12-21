#ifndef PLUGINS_H
#define PLUGINS_H

#include "common.h"

/*
 * Basic plugin functions.
 *
 * All plugins are first allocated with plugin_alloc, then some
 * configuration is done, then initialization.
 */

/*
 * Search for a plugin.
 */
struct agent_plugin_t *plugin_find(struct agent_core_t *core, const char *name);

/*
 * Allocate a plugin and IPC. Does NOT init the plugin.
 */
void plugin_alloc(char *name, struct agent_core_t *core);

/*
 * Init functions for said plugins.
 *
 * Should eventually be dealt with by dlopen etc.
 *
 * These are all run in the context of the main thread. These functions are
 * used to register ipc-interests, url-handlers, log-handlers etc.
 *
 */
void pingd_init(struct agent_core_t *core);
void logd_init(struct agent_core_t *core);
void httpd_init(struct agent_core_t *core);
void echo_init(struct agent_core_t *core);
void status_init(struct agent_core_t *core);
void vcl_init(struct agent_core_t *core);
void html_init(struct agent_core_t *core);
void vadmin_init(struct agent_core_t *core);
#endif


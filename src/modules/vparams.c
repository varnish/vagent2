/*
 * Copyright (c) 2012-2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Kristian Lyngstøl <kristian@bohemians.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE
#include "common.h"
#include "plugins.h"
#include "ipc.h"
#include "http.h"
#include "helpers.h"

#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#define PARAM_HELP \
"GET /param/ - fetches a list of parameters and values\n" \
"GET /param/foo - fetches details for the parameter named foo\n" \
"PUT /param/ - takes a single key=value input (e.g: session_max=1000)\n" \
"PUT /param/foo - Takes a single value as input (e.g: 1000)\n"

struct vparams_priv_t {
	int logger;
	int vadmin;
};

/*
 * Parameter list, used for JSON parsing.
 */
struct param_opt {
	char *name;
	char *value;
	char *unit;
	char *def;
	char *description;
	struct param_opt *next;
};

static void param_assert(struct param_opt *p)
{
	assert(p);
	assert(p->name);
	assert(p->value);
	assert(p->unit);
	assert(p->def);
	assert(p->description);
}

/*
 * Frees the param and returns NEXT
 */
static struct param_opt *param_free(struct param_opt *p)
{
	struct param_opt *next;
	param_assert(p);
	free(p->value);
	free(p->name);
	free(p->unit);
	free(p->def);
	free(p->description);
	next = p->next;
	free(p);
	return next;
}

/*
 * I hate myself and I want to die.
 *
 * This should be cleaned up considerably. Numerous flaws (e.g: word-size
 * mix). Unmanageable. Ugly. Fugly. Etc.
 *
 * Newer Varnish versions makes this redundant.
 * (Parses param.show -l and outputs json. Caller must issue free();
 */
static char *vparams_show_json(char *raw)
{
	char word[2048];
	struct param_opt *tmp, *top;
	int pos = 0, i = 0;
	int state = 0;
	char *term = NULL;
	char *out = NULL, *out2 = NULL, *out3 = NULL;
	tmp = malloc(sizeof (struct param_opt));
	top = NULL;
	tmp->next = NULL;

	while(raw[pos]) {
		word[i++] = raw[pos];
		assert(i<510);
		if (state == 0 && raw[pos] == ' ') {
			word[i-1] = '\0';
			tmp->name = strdup(word);
			i = 0;
			while (raw[pos] == ' ')
				pos++;
			word[0] = raw[pos];
			state = 1;
			pos--;
		} else if (state == 1 && (raw[pos] == '\n' || raw[pos] == '[')) {
			assert(i<510);
			i--;
			word[i] = '\0';
			term = rindex(word,'[');
			if (term) {
				assert(*(term-1) == ' ');
				*(term-1) = '\0';
			}
			i = strlen(word);
			while(word[--i] == ' ');
			if (word[i] == '"') {
				assert(word[0] == '"');
				memmove(word, word+1, i-1);
				i-=2;
			}
			word[i+1] = '\0';
			assert(index(word,'"') == NULL);

			tmp->value = strdup(word);
			i = 0;
			if (raw[pos] == '\n')
				tmp->unit = strdup("");
			else {
				pos++;
				while (raw[pos] != ']')
					word[i++] = raw[pos++];
				word[i] = '\0';
				tmp->unit = strdup(word);
				i = 0;
			}
			if (raw[pos] == '\n')
				pos++;
			term = strstr(raw+pos, "Default is ");
			assert(term);
			pos = term-raw + strlen("Default is ");
			if (raw[pos] == 0x01) {
				assert(raw[pos+1] == '\n');
				pos++;
			}
			while (raw[pos] != '\n')  {
				word[i++] = raw[pos++];
				assert(i<2048);
			}
			assert(raw[pos] == '\n');
			assert((isprint(word[0]) || i == 0));
			word[i] = '\0';
			tmp->def = strdup(word);
			i = 0;
			state = 0;
			while (!(raw[pos-1] == '\n' && !isspace(raw[pos]))) {
				if (raw[pos] == '\n') {
					pos++;
					word[i++] = '\\';
					word[i++] = 'n';
					state = 0;
				} else if (state == 0 && isspace(raw[pos])) {
					pos++;
				} else {
					state = 1;
					if (raw[pos] == '"')
						word[i++] = '\\';
					if (raw[pos] == '\t') {
						word[i++] = '\\';
						word[i++] = 't';
						pos++;
					} else
						word[i++] = raw[pos++];
				}
			}
			word[i] = '\0';
			tmp->description = strdup(word);
			tmp->next = top;
			top = tmp;
			state = 0;
			i = 0;
			assert(strlen(tmp->name) < 30);
			tmp = malloc(sizeof (struct param_opt));
			tmp->next = NULL;
			pos--;
		}
		pos++;
	}
	state = asprintf(&out3, "{\n");
	assert(state);
	for (tmp = top; tmp != NULL; ) {
		param_assert(tmp);
		state = asprintf(&out, "\t\"%s\": {\n"
			"\t\t\"value\": \"%s\",\n"
			"\t\t\"default\": \"%s\",\n"
			"\t\t\"unit\": \"%s\",\n"
			"\t\t\"description\": \"%s\"\n"
			"\t}",
			tmp->name, tmp->value, tmp->def,
			tmp->unit, tmp->description);
		assert(state);
		state = asprintf(&out2, "%s%s%s", out3, out, (tmp->next) ? ",\n":"");
		assert(state);
		free(out);
		free(out3);
		out3 = out2;
		tmp = param_free(tmp);
	}
	state = asprintf(&out2, "%s\n}\n", out3);
	free(out3);
	return out2;
}

static void param_json(struct http_request *request, struct vparams_priv_t *vparams)
{
	struct ipc_ret_t vret;
	char *tmp;
	ipc_run(vparams->vadmin, &vret, "param.show -l");
	if (vret.status == 200) {
		tmp = vparams_show_json(vret.answer);
		struct http_response *resp = http_mkresp(request->connection, 200, tmp);
		http_add_header(resp,"Content-Type","application/json");
		send_response2(resp);
		free(tmp);
		http_free_resp(resp);
	} else {
		send_response_fail(request->connection, vret.answer);
	}
	free(vret.answer);
}

/*
 * FIXME: Should be simplified/split up.
 */
static unsigned int vparams_reply(struct http_request *request, void *data)
{
	const char *arg;
	struct agent_core_t *core = data;
	struct agent_plugin_t *plug;
	struct vparams_priv_t *vparams;
	char *body;
	plug = plugin_find(core,"vparams");
	vparams = plug->data;

	if (!strcmp(request->url, "/paramjson/") && request->method == M_GET) {
		param_json(request, vparams);
		return 1;
	}
	if (request->method == M_GET) {
		if (!strcmp(request->url,"/param/")) {
			run_and_respond(vparams->vadmin,
				request->connection,
				"param.show");
			return 1;
		} else {
			arg = request->url + strlen("/param/");
			assert(arg && *arg);
			run_and_respond(vparams->vadmin,
				request->connection,
				"param.show %s", arg);
			return 1;
		}
	} else if (request->method == M_PUT) {
		char *mark;
		assert(((char *)request->data)[request->ndata] == '\0');
		body = strdup(request->data);
		mark = index(body,'\n');
		if (mark)
			*mark = '\0';
		if (!strcmp(request->url, "/param/")) {
			run_and_respond(vparams->vadmin,
				request->connection,
				"param.set %s",body);
		} else {
			arg = request->url + strlen("/param/");
			assert(arg && *arg);
			run_and_respond(vparams->vadmin,
				request->connection,
				"param.set %s %s",arg, body);
		}
		free(body);
		return 1;

	}
	send_response_fail(request->connection, "Failed");
	return 1;
}

void
vparams_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vparams_priv_t *priv = malloc(sizeof(struct vparams_priv_t));
	plug = plugin_find(core,"vparams");

	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
	http_register_url(core, "/param/", M_PUT | M_GET, vparams_reply, core);
	http_register_url(core, "/paramjson/", M_GET, vparams_reply, core);
	http_register_url(core, "/help/param", M_GET, help_reply, strdup(PARAM_HELP));
}

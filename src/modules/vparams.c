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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "http.h"
#include "helpers.h"
#include "ipc.h"
#include "plugins.h"


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
	char *min;
	char *max;
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
	assert(p->min);
	assert(p->max);
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
	free(p->min);
	free(p->max);
	next = p->next;
	free(p);
	return next;
}

/*
 * Parse a hopefully generic 'name'-string (first string).
 *
 * Returns a malloc'ed param_opt.
 *
 * Some fields are optional: default and unit MIGHT be filled in, depending
 * on output.
 *
 * <variable> [value] [unit]\n
 */
static struct param_opt *get_name_val(const char *raw)
{
	struct param_opt *r = calloc(1,sizeof(struct param_opt));
	char *line = strdup(raw);
	char *tmp = strchr(line,'\n');
	char *tmp2;
	assert(tmp);
	*tmp = '\0';

	tmp = strchr(line,' ');
	if (tmp)
		*tmp = '\0';
	r->name = strdup(line);
	assert(r->name);
	assert(*(r->name));
	if (!tmp) {
		free(line);
		return r;
	}
	tmp++;
	tmp2 = strchr(tmp,' ');
	if (tmp2)
		*tmp2 = '\0';
	r->value = strdup(tmp);
	if (!tmp2) {
		free(line);
		return r;
	}
	r->unit = strdup(tmp2+1);
	free(line);
	return r;
}

/*
 * Just skip space characters to avoid indentation meh.
 */
static char *skip_space(char *p) {
	while (*p && (*p == ' ' || *p == '\t'))
		p++;
	return p;
}

/*
 * Prase:
 *
 * Value is: <value> [\[unit\]] [(default)]
 *
 */
static int parse_value(const char *w, struct param_opt *p) {
	char *tmp = strdup(w);
	char *tmp2;
	char *orig;
	orig = tmp;
	tmp2 = strchr(tmp,'\n');
	assert(tmp2);
	*tmp2 = '\0';

	if (*tmp == '"') {
		tmp2 = strchr(tmp+1,'"');
		tmp2++;
	} else {
		tmp2 = strchr(tmp, ' ');
	}
	if (tmp2) {
		*tmp2 = '\0';
	}
	assert(p->value == NULL);
	p->value = strdup(tmp);
	if (!tmp2) {
		free(orig);
		return 0;
	}
	tmp = tmp2+1;
	if (*tmp == '[') {
		int mark = 0;
		tmp2 = strchr(tmp,']');
		assert(tmp2);
		tmp2++;
		if (*tmp2 == ' ')
			mark = 1;
		*tmp2 = '\0';
		p->unit = strdup(tmp);
		if (mark)
			tmp = tmp2+1;
	}
	if (*tmp == '(') {
		p->def = strdup(p->value);
	}
	return 1;
}

/*
 * Parse an entry after we have the name (and possibly more).
 */
static char *fill_entry(struct param_opt *p, const char *pos)
{
	char *tmp;
	char *tmp2;
	assert(*pos);
	tmp = skip_space(strdup(pos));
	assert(tmp);
	if (!strncmp("Value is: ", tmp, strlen("Value is: "))) {
		parse_value(tmp+strlen("Value is: "), p);
		tmp = strchr(tmp, '\n');
		assert(tmp);
		tmp++;
		tmp = skip_space(tmp);
	}
	if (!strncmp("Default is: ", tmp, strlen("Default is: "))) {
		tmp2 = strchr(tmp,'\n');
		assert(tmp2);
		*tmp2 = '\0';
		p->def = strdup(tmp);
		tmp = tmp2+1;
		tmp = skip_space(tmp);
	}
	if (!strncmp("Minimum is: ", tmp, strlen("Minimum is: "))) {
		tmp2 = strchr(tmp,'\n');
		assert(tmp2);
		*tmp2 = '\0';
		p->min = strdup(tmp);
		tmp = tmp2+1;
		tmp = skip_space(tmp);
	}
	if (!strncmp("Maximum is: ", tmp, strlen("Maximum is: "))) {
		tmp2 = strchr(tmp,'\n');
		assert(tmp2);
		*tmp2 = '\0';
		p->max = strdup(tmp);
		tmp = tmp2+1;
		tmp = skip_space(tmp);
	}
	char desc[2048];
	desc[0] = '\0';
	if (*tmp == '\n')
		tmp++;
	while(1) {
		tmp = skip_space(tmp);
		tmp2 = strchr(tmp,'\n');	
		assert(tmp2);
		*tmp2 = '\0';
		strncat(desc,tmp,2047);
		strncat(desc," ",2047);
		tmp = tmp2+1;
		if (*tmp != ' ' && *tmp != '\n')
			break;
	}
	p->description = strdup(desc);
	return tmp;
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
	struct param_opt *tmp, *top;
	char *out = NULL, *out2 = NULL, *out3 = NULL;
	int state = 0;
	tmp = malloc(sizeof (struct param_opt));
	top = NULL;
	tmp->next = NULL;

	/*
	 * param.show -l output:
	 *
	 * V3:
	 * (parameter) (value) \[(unit|)\]
	 *             Default is (value)
	 *             (description line 1)
	 *             ..
	 *             (description line n)
	 *
	 * V4/4.1:
	 * (parameter)
	 *   Value is: (value) [\[(unit)\]] [\(default\)]
	 *   [Default is: (value)]
	 *   [Minimum is: (value)]
	 *   [Maximum is: (value)]
	 *
	 *   (description line 1)
	 *   ..
	 *   (description line n)
	 */

	char *pos = raw;
	while(pos && *pos) {
		tmp = get_name_val(pos);
		assert(tmp);
		pos = strchr(pos, '\n');
		assert(pos);
		pos++;
		pos = fill_entry(tmp, pos);
		tmp->next = top;
		top = tmp;
		/*
		 * Sort of silly, but simplifies cleanup
		 */
		if (!tmp->unit) {
			tmp->unit = strdup("");
		}
		if (!tmp->min) {
			tmp->min = strdup("");
		}
		if (!tmp->max) {
			tmp->max = strdup("");
		}
		param_assert(tmp);
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
	const char *param = (request->url) + strlen("/paramjson/");
	ipc_run(vparams->vadmin, &vret, "param.show %s", *param ? param : "-l");
	if (vret.status == 200) {
		tmp = vparams_show_json(vret.answer);
		struct http_response *resp = http_mkresp(request->connection, 200, tmp);
		http_add_header(resp,"Content-Type","application/json");
		send_response(resp);
		free(tmp);
		http_free_resp(resp);
	} else {
		http_reply(request->connection, 500, vret.answer);
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
	struct vparams_priv_t *vparams;
	char *body;

	GET_PRIV(core, vparams);

	if (!strncmp(request->url, "/paramjson/", strlen("/paramjson/")) && request->method == M_GET) {
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
		mark = strchr(body,'\n');
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
	http_reply(request->connection, 500, "Failed");
	return 1;
}

void
vparams_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vparams_priv_t *priv;

	ALLOC_OBJ(priv);
	plug = plugin_find(core,"vparams");

	priv->logger = ipc_register(core,"logger");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	http_register_url(core, "/param/", M_PUT | M_GET, vparams_reply, core);
	http_register_url(core, "/paramjson/", M_GET, vparams_reply, core);
	http_register_url(core, "/help/param", M_GET, help_reply, strdup(PARAM_HELP));
}

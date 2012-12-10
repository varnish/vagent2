#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>


#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vapi/vsl.h"
#include "vapi/vsm.h"
#include "vas.h"
#include "vcli.h"
#include "vss.h"

#include "varnishadm.h"
#include "main.h"

#define RL_EXIT(s) exit(s)


static int
cli_write(int sock, const char *s)
{
	int i, l;

	i = strlen(s);
	l = write (sock, s, i);
	if (i == l)
		return 1;
	perror("Write error CLI socket");
	AZ(close(sock));
	return 0;
}

/*
 * This function establishes a connection to the specified ip and port and
 * sends a command to varnishd. If varnishd returns an OK status, the result
 * is printed and 0 returned. Else, an error message is printed and 1 is
 * returned
 */
static int
cli_sock(struct vadmin_config_t *vadmin)
{
	int fd;
	unsigned status;
	char *answer = NULL;
	char buf[CLI_AUTH_RESPONSE_LEN + 1];

	vadmin->sock = VSS_open(vadmin->T_arg, vadmin->timeout);
	if (vadmin->sock < 0) {
		fprintf(stderr, "Connection failed (%s)\n", vadmin->T_arg);
		return (-1);
	}

	(void)VCLI_ReadResult(vadmin->sock, &status, &answer, vadmin->timeout);
	if (status == CLIS_AUTH) {
		if (vadmin->S_arg == NULL) {
			fprintf(stderr, "Authentication required\n");
			AZ(close(vadmin->sock));
			return(-1);
		}
		fd = open(vadmin->S_arg, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "Cannot open \"%s\": %s\n",
			    vadmin->S_arg, strerror(errno));
			AZ(close(vadmin->sock));
			return (-1);
		}
		VCLI_AuthResponse(fd, answer, buf);
		AZ(close(fd));
		free(answer);

		cli_write(vadmin->sock, "auth ");
		cli_write(vadmin->sock, buf);
		cli_write(vadmin->sock, "\n");
		(void)VCLI_ReadResult(vadmin->sock, &status, &answer, vadmin->timeout);
	}
	if (status != CLIS_OK) {
		fprintf(stderr, "Rejected %u\n%s\n", status, answer);
		AZ(close(vadmin->sock));
		return (-1);
	}
	free(answer);

	cli_write(vadmin->sock, "ping\n");
	(void)VCLI_ReadResult(vadmin->sock, &status, &answer, vadmin->timeout);
	if (status != CLIS_OK || strstr(answer, "PONG") == NULL) {
		fprintf(stderr, "No pong received from server\n");
		AZ(close(vadmin->sock));
		return(-1);
	}
	free(answer);

	return (vadmin->sock);
}

void
vadmin_run(int sock, char *cmd, struct vadmin_ret *ret)
{
	assert(cmd);
	cli_write(sock, cmd);
	cli_write(sock, "\n");

	(void)VCLI_ReadResult(sock, &ret->status, &ret->answer, 2000);
}

int
read_cmd(int fd, struct vadmin_config_t *vadmin)
{
	char buf[1024];
	char c;
	int i, iret;
	struct vadmin_ret ret;

	for (i=0; i<1024; i++) {
		iret = read(fd, &c, 1);
		assert (iret == 1);
		buf[i] = c;
		if (c == '\n')
			break;
	}
	assert(i < 1024);
	buf[i] = '\0';
	vadmin_run(vadmin->sock, buf, &ret);
	VCLI_WriteResult(fd, ret.status, ret.answer);
	return 1;
}

/*
 * No arguments given, simply pass bytes on stdin/stdout and CLI socket
 * Send a "banner" to varnish, to provoke a welcome message.
 */
void *
pass(void *data)
{
	struct vadmin_config_t *vadmin = data;
	struct pollfd fds[MAX_LISTENERS+1];
	int i;
	int ret;
	int n;

//	cli_write(vadmin->sock, "banner\n");
	for (i=0; i < vadmin->nlisteners; i ++) {
		fds[i].fd = vadmin->listeners[i];
		fds[i].events = POLLIN;
	}
	while (1) {
		ret = poll(fds, vadmin->nlisteners, -1);
		assert(ret > 0);
		for (i=0; i < vadmin->nlisteners; i++) {
			if (fds[i].revents & POLLIN) {
				n = read_cmd(fds[i].fd, vadmin);
			}
		}
	}
}

static int
n_arg_sock(struct vadmin_config_t *vadmin)
{
	char *T_start;
	struct VSM_data *vsd;
	char *p;
	struct VSM_fantom vt;

	vsd = VSM_New();
	assert(VSL_Arg(vsd, 'n', vadmin->n_arg));
	if (VSM_Open(vsd)) {
		fprintf(stderr, "%s\n", VSM_Error(vsd));
		return (-1);
	}

	if (!VSM_Get(vsd, &vt, "Arg", "-T", "")) {
		fprintf(stderr, "No -T arg in shared memory\n");
		return (-1);
	}
	AN(vt.b);
	T_start = vadmin->T_arg = strdup(vt.b);

	if (VSM_Get(vsd, &vt, "Arg", "-S", "")) {
		AN(vt.b);
		vadmin->S_arg = strdup(vt.b);
	}

	
	p = strchr(vadmin->T_arg, '\n');
	AN(p);
	*p = '\0';
	return (1);
}

struct vadmin_config_t *
vadmin_preconf(void)
{
	struct vadmin_config_t *vadmin;
	vadmin = malloc(sizeof(struct vadmin_config_t));

	vadmin->n_arg = NULL;
	vadmin->S_arg = NULL;
	vadmin->T_arg = NULL;
	vadmin->timeout = 5;
	vadmin->sock = -1;
	vadmin->nlisteners = 0;
	return vadmin;
}

int
vadmin_register(struct agent_core_t *core)
{
	struct vadmin_config_t *vadmin = core->vadmin;
	int sv[2];
	int ret;

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	assert(ret == 0);
	assert(vadmin->nlisteners < MAX_LISTENERS);
	vadmin->listeners[vadmin->nlisteners++] = sv[0];
	return sv[1];
}

int
vadmin_start(struct vadmin_config_t *vadmin)
{
	pthread_t thread;
	
	pthread_create(&thread,NULL,(pass),vadmin);

	return 1;
}
int
vadmin_init(struct vadmin_config_t *vadmin)
{
	if (vadmin->n_arg != NULL) {
		if (vadmin->T_arg != NULL || vadmin->S_arg != NULL) {
			return -1;
		}
		n_arg_sock(vadmin);
	} else if (vadmin->T_arg == NULL) {
		vadmin->n_arg = "";
		n_arg_sock(vadmin);
	} else {
		assert(vadmin->T_arg != NULL);
	}
	cli_sock(vadmin);
	if (vadmin->sock < 0)
		exit(2);
		
	return (1);
}

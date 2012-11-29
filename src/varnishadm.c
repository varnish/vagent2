#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>


#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vapi/vsl.h"
#include "vapi/vsm.h"
#include "vas.h"
#include "vcli.h"
#include "vss.h"

#include "varnishadm.h"
#define RL_EXIT(s) exit(s)

static double timeout = 5;

static void
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
cli_sock(const char *T_arg, const char *S_arg)
{
	int fd;
	int sock;
	unsigned status;
	char *answer = NULL;
	char buf[CLI_AUTH_RESPONSE_LEN + 1];

	sock = VSS_open(T_arg, timeout);
	if (sock < 0) {
		fprintf(stderr, "Connection failed (%s)\n", T_arg);
		return (-1);
	}

	(void)VCLI_ReadResult(sock, &status, &answer, timeout);
	if (status == CLIS_AUTH) {
		if (S_arg == NULL) {
			fprintf(stderr, "Authentication required\n");
			AZ(close(sock));
			return(-1);
		}
		fd = open(S_arg, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "Cannot open \"%s\": %s\n",
			    S_arg, strerror(errno));
			AZ(close(sock));
			return (-1);
		}
		VCLI_AuthResponse(fd, answer, buf);
		AZ(close(fd));
		free(answer);

		cli_write(sock, "auth ");
		cli_write(sock, buf);
		cli_write(sock, "\n");
		(void)VCLI_ReadResult(sock, &status, &answer, timeout);
	}
	if (status != CLIS_OK) {
		fprintf(stderr, "Rejected %u\n%s\n", status, answer);
		AZ(close(sock));
		return (-1);
	}
	free(answer);

	cli_write(sock, "ping\n");
	(void)VCLI_ReadResult(sock, &status, &answer, timeout);
	if (status != CLIS_OK || strstr(answer, "PONG") == NULL) {
		fprintf(stderr, "No pong received from server\n");
		AZ(close(sock));
		return(-1);
	}
	free(answer);

	return (sock);
}

void
vadmin_run(int sock, char *cmd, struct vadmin_ret *ret)
{
	assert(cmd);
	cli_write(sock, cmd);
	cli_write(sock, "\n");

	(void)VCLI_ReadResult(sock, &ret->status, &ret->answer, 2000);

}

/*
 * No arguments given, simply pass bytes on stdin/stdout and CLI socket
 * Send a "banner" to varnish, to provoke a welcome message.
 */
void
pass(int sock)
{
	struct pollfd fds[2];
	char buf[1024];
	int i;
	char *answer = NULL;
	unsigned u, status;
	int n;

	cli_write(sock, "banner\n");
	fds[0].fd = sock;
	fds[0].events = POLLIN;
	fds[1].fd = 0;
	fds[1].events = POLLIN;
	while (1) {
		i = poll(fds, 2, -1);
		assert(i > 0);
		if (fds[0].revents & POLLIN) {
			/* Get rid of the prompt, kinda hackish */
			u = write(1, "\r           \r", 13);
			u = VCLI_ReadResult(fds[0].fd, &status, &answer,
			    timeout);
			if (u) {
				if (status == CLIS_COMMS)
					RL_EXIT(0);
				if (answer)
					fprintf(stderr, "%s\n", answer);
				RL_EXIT(1);
			}

			sprintf(buf, "%u\n", status);
			u = write(1, buf, strlen(buf));
			if (answer) {
				u = write(1, answer, strlen(answer));
				u = write(1, "\n", 1);
				free(answer);
				answer = NULL;
			}
		}
		if (fds[1].revents & POLLIN) {
			n = read(fds[1].fd, buf, sizeof buf);
			if (n == 0) {
				AZ(shutdown(sock, SHUT_WR));
				fds[1].fd = -1;
			} else if (n < 0) {
				RL_EXIT(0);
			} else {
				buf[n] = '\0';
				cli_write(sock, buf);
			}
		}
	}
}

static int
n_arg_sock(const char *n_arg)
{
	char *T_arg = NULL, *T_start = NULL;
	char *S_arg = NULL;
	struct VSM_data *vsd;
	char *p;
	int sock;
	struct VSM_fantom vt;

	vsd = VSM_New();
	assert(VSL_Arg(vsd, 'n', n_arg));
	if (VSM_Open(vsd)) {
		fprintf(stderr, "%s\n", VSM_Error(vsd));
		return (-1);
	}

	if (!VSM_Get(vsd, &vt, "Arg", "-T", "")) {
		fprintf(stderr, "No -T arg in shared memory\n");
		return (-1);
	}
	AN(vt.b);
	T_start = T_arg = strdup(vt.b);

	if (VSM_Get(vsd, &vt, "Arg", "-S", "")) {
		AN(vt.b);
		S_arg = strdup(vt.b);
	}

	sock = -1;
	while (*T_arg) {
		p = strchr(T_arg, '\n');
		AN(p);
		*p = '\0';
		sock = cli_sock(T_arg, S_arg);
		if (sock >= 0)
			break;
		T_arg = p + 1;
	}
	free(T_start);
	free(S_arg);
	return (sock);
}

int
vadmin_init(int argc, char **argv)
{
	const char *T_arg = NULL;
	const char *S_arg = NULL;
	const char *n_arg = NULL;
	int opt, sock;

	while ((opt = getopt(argc, argv, "n:S:T:t:")) != -1) {
		switch (opt) {
		case 'n':
			n_arg = optarg;
			break;
		case 'S':
			S_arg = optarg;
			break;
		case 'T':
			T_arg = optarg;
			break;
		case 't':
			timeout = strtod(optarg, NULL);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (n_arg != NULL) {
		if (T_arg != NULL || S_arg != NULL) {
			usage();
		}
		sock = n_arg_sock(n_arg);
	} else if (T_arg == NULL) {
		sock = n_arg_sock("");
	} else {
		assert(T_arg != NULL);
		sock = cli_sock(T_arg, S_arg);
	}
	if (sock < 0)
		exit(2);
	return (sock);
}

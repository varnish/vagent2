/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Dag-Erling Smørgrav <des@des.no>
 * Author: Cecilie Fritzvold <cecilihf@linpro.no>
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
 *
 */

/*
 * XXX: This is a direct copy/paste from varnish' lib/libvarnish/vss.c,
 * with unused code removed. It's borrowed since VSS_connect is currently
 * not exposed and we want to mimick varnishadm as closely as possible.
 * 
 * FIXME: Remove this file. Use proper libraries.
 *
 * Thief: Kristian Lyngstol <kristian@bohemians.org>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <assert.h>

#include "vss-hack.h"

/* lightweight addrinfo */
struct vss_addr {
	int			 va_family;
	int			 va_socktype;
	int			 va_protocol;
	socklen_t		 va_addrlen;
	struct sockaddr_storage	 va_addr;
};

/*lint -esym(754, _storage) not ref */

/*
 * Take a string provided by the user and break it up into address and
 * port parts.  Examples of acceptable input include:
 *
 * "localhost" - "localhost:80"
 * "127.0.0.1" - "127.0.0.1:80"
 * "0.0.0.0" - "0.0.0.0:80"
 * "[::1]" - "[::1]:80"
 * "[::]" - "[::]:80"
 *
 * See also RFC5952
 */

static int
VSS_parse(const char *str, char **addr, char **port)
{
	const char *p;

	*addr = *port = NULL;

	if (str[0] == '[') {
		/* IPv6 address of the form [::1]:80 */
		if ((p = strchr(str, ']')) == NULL ||
		    p == str + 1 ||
		    (p[1] != '\0' && p[1] != ':'))
			return (-1);
		*addr = strdup(str + 1);
		assert(*addr);
		(*addr)[p - (str + 1)] = '\0';
		if (p[1] == ':') {
			*port = strdup(p + 2);
			assert(*port);
		}
	} else {
		/* IPv4 address of the form 127.0.0.1:80, or non-numeric */
		p = strchr(str, ' ');
		if (p == NULL)
			p = strchr(str, ':');
		if (p == NULL) {
			*addr = strdup(str);
			assert(*addr);
		} else {
			if (p > str) {
				*addr = strdup(str);
				assert(*addr);
				(*addr)[p - str] = '\0';
			}
			*port = strdup(p + 1);
			assert(*port);
		}
	}
	return (0);
}

/*
 * For a given host and port, return a list of struct vss_addr, which
 * contains all the information necessary to open and bind a socket.  One
 * vss_addr is returned for each distinct address returned by
 * getaddrinfo().
 *
 * The value pointed to by the tap parameter receives a pointer to an
 * array of pointers to struct vss_addr.  The caller is responsible for
 * freeing each individual struct vss_addr as well as the array.
 *
 * The return value is the number of addresses resoved, or zero.
 *
 * If the addr argument contains a port specification, that takes
 * precedence over the port argument.
 *
 * XXX: We need a function to free the allocated addresses.
 */
static int
VSS_resolve(const char *addr, const char *port, struct vss_addr ***vap)
{
	struct addrinfo hints, *res0, *res;
	struct vss_addr **va;
	int i, ret;
	long int ptst;
	char *adp, *hop;

	*vap = NULL;
	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	ret = VSS_parse(addr, &hop, &adp);
	if (ret)
		return (0);

	if (adp == NULL)
		ret = getaddrinfo(addr, port, &hints, &res0);
	else {
		ptst = strtol(adp,NULL,10);
		if (ptst < 0 || ptst > 65535)
			return(0);
		ret = getaddrinfo(hop, adp, &hints, &res0);
	}

	free(hop);
	free(adp);

	if (ret != 0)
		return (0);

	assert(res0);
	for (res = res0, i = 0; res != NULL; res = res->ai_next, ++i)
		/* nothing */ ;
	if (i == 0) {
		freeaddrinfo(res0);
		return (0);
	}
	va = calloc(i, sizeof *va);
	assert(va);
	*vap = va;
	for (res = res0, i = 0; res != NULL; res = res->ai_next, ++i) {
		va[i] = calloc(1, sizeof(**va));
		assert(va[i]);
		va[i]->va_family = res->ai_family;
		va[i]->va_socktype = res->ai_socktype;
		va[i]->va_protocol = res->ai_protocol;
		va[i]->va_addrlen = res->ai_addrlen;
		assert(va[i]->va_addrlen <= sizeof va[i]->va_addr);
		memcpy(&va[i]->va_addr, res->ai_addr, va[i]->va_addrlen);
	}
	freeaddrinfo(res0);
	return (i);
}

/*
 * Connect to the socket specified by the address info in va.
 * Return the socket.
 * XXX: Note that the non-blocking part is removed from the original vss.c
 * XXX: since it was not used. -Kristian
 */
static int
VSS_connect(const struct vss_addr *va)
{
	int sd, i;

	sd = socket(va->va_family, va->va_socktype, va->va_protocol);
	if (sd < 0) {
		if (errno != EPROTONOSUPPORT)
			perror("socket()");
		return (-1);
	}
	i = connect(sd, (const void *)&va->va_addr, va->va_addrlen);
	if (i == 0)
		return (sd);
	perror("connect()");
	(void)close(sd);
	return (-1);
}

/*
 * And the totally brutal version: Give me connection to this address
 */
int
VSS_open(const char *str, double tmo)
{
	int retval = -1;
	int nvaddr, n, i;
	struct vss_addr **vaddr;
	struct pollfd pfd;

	nvaddr = VSS_resolve(str, NULL, &vaddr);
	for (n = 0; n < nvaddr; n++) {
		retval = VSS_connect(vaddr[n]);
		if (retval >= 0 && tmo != 0.0) {
			pfd.fd = retval;
			pfd.events = POLLOUT;
			i = poll(&pfd, 1, tmo * 1e3);
			if (i == 0 || pfd.revents != POLLOUT) {
				(void)close(retval);
				retval = -1;
			}
		}
		if (retval >= 0)
			break;
	}
	for (n = 0; n < nvaddr; n++)
		free(vaddr[n]);
	free(vaddr);
	return (retval);
}

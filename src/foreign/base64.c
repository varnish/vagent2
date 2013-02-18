/*-
 * Copyright (c) 2011,2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Kristian Lyngstøl <kristian@varnish-cache.org>
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

/*
 * Borrowed from libvmod-digest. Author: Kristian Lyngstol
 */

#include <stdio.h>
#include "base64.h"
#include "common.h"

static struct e_alphabet alphabet[N_ALPHA];

static void
digest_alpha_init(struct e_alphabet *alpha)
{
	int i;
	const char *p;

	for (i = 0; i < 256; i++)
		alpha->i64[i] = -1;
	for (p = alpha->b64, i = 0; *p; p++, i++)
		alpha->i64[(int)*p] = (char)i;
	if (alpha->padding)
		alpha->i64[(unsigned char)alpha->padding] = 0;
}

int
base64_init(void)
{
    	alphabet[BASE64].b64 =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		"ghijklmnopqrstuvwxyz0123456789+/";
	alphabet[BASE64].padding = '=';
	alphabet[BASE64URL].b64 =
		 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		 "ghijklmnopqrstuvwxyz0123456789-_";
	alphabet[BASE64URL].padding = '=';
	alphabet[BASE64URLNOPAD].b64 =
		 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		 "ghijklmnopqrstuvwxyz0123456789-_";
	alphabet[BASE64URLNOPAD].padding = 0;
	digest_alpha_init(&alphabet[BASE64]);
	digest_alpha_init(&alphabet[BASE64URL]);
	digest_alpha_init(&alphabet[BASE64URLNOPAD]);
	return (0);
}

/*
 * Decodes the string s into the buffer d (size dlen), using the alphabet
 * specified.
 *
 * Modified slightly from varnishncsa's decoder. Mainly because the
 * input-length is known, so padding is optional (this is per the RFC and
 * allows this code to be used regardless of whether padding is present).
 * Also returns the length of data when it succeeds.
 */
int
base64_decode(enum alphabets al, char *d, unsigned dlen, const char *s)
{
	unsigned u, v, l;
	int i;
	struct e_alphabet *alpha = &alphabet[al];

	u = 0;
	l = 0;
	while (*s) {
		for (v = 0; v < 4; v++) {
			if (*s)
				i = alpha->i64[(int)*s++];
			else
				i = 0;
			if (i < 0)
				return (-1);
			u <<= 6;
			u |= i;
		}
		for (v = 0; v < 3; v++) {
			if (l >= dlen - 1)
				return (-1);
			*d = (u >> 16) & 0xff;
			u <<= 8;
			l++;
			d++;
		}
		if (!*s)
			break;
	}
	*d = '\0';
	l++;
	return (l);
}

/*
 * Base64-encode *in (size: inlen) into *out, max outlen bytes. If there is
 * insufficient space, it will bail out and return -1. Otherwise, it will
 * null-terminate and return the used space.
 * The alphabet `a` defines... the alphabet. Padding is optional.
 * Inspired heavily by gnulib/Simon Josefsson (as referenced in RFC4648)
 *
 * XXX: tmp[] and idx are used to ensure the reader (and author) retains
 * XXX: a limited amount of sanity. They are strictly speaking not
 * XXX: necessary, if you don't mind going crazy.
 *
 * FIXME: outlenorig is silly. Flip the logic.
 */
size_t
base64_encode (enum alphabets al, const char *in,
		size_t inlen, char *out, size_t outlen)
{
	size_t outlenorig = outlen;
	unsigned char tmp[3], idx;
	struct e_alphabet *alpha = &alphabet[al];

	if (outlen<4)
		return -1;

	if (inlen == 0) {
		*out = '\0';
		return (1);
	}

	while (1) {
		assert(inlen);
		assert(outlen>3);

		tmp[0] = (unsigned char) in[0];
		tmp[1] = (unsigned char) in[1];
		tmp[2] = (unsigned char) in[2];

		*out++ = alpha->b64[(tmp[0] >> 2) & 0x3f];

		idx = (tmp[0] << 4);
		if (inlen>1)
			idx += (tmp[1] >> 4);
		idx &= 0x3f;
		*out++ = alpha->b64[idx];
			
		if (inlen>1) {
			idx = (tmp[1] << 2);
			if (inlen>2)
				idx += tmp[2] >> 6;
			idx &= 0x3f;

			*out++ = alpha->b64[idx];
		} else {
			if (alpha->padding)
				*out++ = alpha->padding;
		}

		if (inlen>2) {
			*out++ = alpha->b64[tmp[2] & 0x3f];
		} else {
			if (alpha->padding)
				*out++ = alpha->padding;
		}

		/*
		 * XXX: Only consume 4 bytes, but since we need a fifth for
		 * XXX: NULL later on, we might as well test here.
		 */
		if (outlen<5)
			return -1;

		outlen -= 4;

		if (inlen<4)
			break;
		
		inlen -= 3;
		in += 3;
	}

	assert(outlen);
	outlen--;
	*out = '\0';
	return outlenorig-outlen;
}

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

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "base64.h"
#include "common.h"
#include "vsb.h"

struct e_alphabet {
	const char *b64;
	char i64[256];
	char padding;
} alphabet[N_ALPHA];

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

struct vsb *
base64_encode(enum alphabets al, const char *in)
{
	struct vsb *out;
	unsigned char t[3];
	size_t inlen;
	struct e_alphabet *alpha = &alphabet[al];
	int outlen;

	assert(al < N_ALPHA);
	AN(in);

	inlen = strlen(in);
	out = VSB_new_auto();
	AN(out);
	outlen = 0;

	while (inlen > 0) {

		t[outlen] = (unsigned char)*in;
		in++;
		inlen--;
		outlen++;

		if (outlen == 3) {
			AZ(VSB_putc(out, alpha->b64[t[0] >> 2]));
			AZ(VSB_putc(out,
			    alpha->b64[(t[0] & 0x03) << 4 | (t[1] >> 4)]));
			AZ(VSB_putc(out,
			    alpha->b64[(t[1] & 0x0f) << 2 | (t[2] >> 6)]));
			AZ(VSB_putc(out, alpha->b64[t[2] & 0x3f]));
			outlen = 0;
		}
	}

	if (outlen > 0) {
		if (outlen == 1)
			t[1] = '\0';

		AZ(VSB_putc(out, alpha->b64[t[0] >> 2]));
		AZ(VSB_putc(out, alpha->b64[(t[0] & 0x03) << 4 | (t[1] >> 4)]));

		if (outlen == 2)
			AZ(VSB_putc(out, alpha->b64[(t[1] & 0x0f) << 2]));
		else
			AZ(VSB_putc(out, alpha->padding));

		AZ(VSB_putc(out, alpha->padding));
	}

	AZ(VSB_putc(out, '\0'));
	AZ(VSB_finish(out));

	return (out);
}

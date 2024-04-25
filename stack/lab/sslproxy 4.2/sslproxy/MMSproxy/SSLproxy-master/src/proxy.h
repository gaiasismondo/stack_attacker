/*-
 * SSLsplit - transparent SSL/TLS interception
 * https://www.roe.ch/SSLsplit
 *
 * Copyright (c) 2009-2019, Daniel Roethlisberger <daniel@roe.ch>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PROXY_H
#define PROXY_H

#include "opts.h"
#include "attrib.h"
#include "pxythrmgr.h"

#include <sys/syslog.h>

typedef struct proxy_ctx proxy_ctx_t;

/*
 * Listener context.
 */
typedef struct proxy_listener_ctx {
	pxy_thrmgr_ctx_t *thrmgr;
	proxyspec_t *spec;
	global_t *global;
#ifndef WITHOUT_USERAUTH
	evutil_socket_t clisock;
#endif /* !WITHOUT_USERAUTH */
	struct evconnlistener *evcl;
	struct proxy_listener_ctx *next;
} proxy_listener_ctx_t;

proxy_ctx_t * proxy_new(global_t *, int) NONNULL(1) MALLOC;
int proxy_run(proxy_ctx_t *) NONNULL(1);
void proxy_loopbreak(proxy_ctx_t *, int) NONNULL(1);
void proxy_free(proxy_ctx_t *) NONNULL(1);
void proxy_listener_errorcb(struct evconnlistener *, UNUSED void *);

pxy_conn_ctx_t *proxy_conn_ctx_new(evutil_socket_t, pxy_thrmgr_ctx_t *, proxyspec_t *, global_t *
#ifndef WITHOUT_USERAUTH
	, evutil_socket_t
#endif /* !WITHOUT_USERAUTH */
	) MALLOC NONNULL(2,3,4);
#endif /* !PROXY_H */

/* vim: set noet ft=c: */

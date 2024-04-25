/*-
 * SSLsplit - transparent SSL/TLS interception
 * https://www.roe.ch/SSLsplit
 *
 * Copyright (c) 2009-2019, Daniel Roethlisberger <daniel@roe.ch>.
 * Copyright (c) 2017-2021, Soner Tari <sonertari@gmail.com>.
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

#include "protoautossl.h"
#include "prototcp.h"
#include "protossl.h"

#include <string.h>
#include <sys/param.h>
#include <event2/bufferevent_ssl.h>

typedef struct protoautossl_ctx protoautossl_ctx_t;

struct protoautossl_ctx {
	unsigned int clienthello_search : 1;       /* 1 if waiting for hello */
	unsigned int clienthello_found : 1;      /* 1 if conn upgrade to SSL */
};

static void NONNULL(1)
protoautossl_upgrade_dst_child(pxy_conn_child_ctx_t *ctx)
{
	if (protossl_setup_dst_ssl_child(ctx) == -1) {
		return;
	}
	if (protossl_setup_dst_new_bev_ssl_connecting_child(ctx) == -1) {
		return;
	}
	bufferevent_setcb(ctx->dst.bev, pxy_bev_readcb_child, pxy_bev_writecb_child, pxy_bev_eventcb_child, ctx);
}

/*
 * Peek into pending data to see if it is an SSL/TLS ClientHello, and if so,
 * upgrade the connection from plain TCP to SSL/TLS.
 *
 * Return 1 if ClientHello was found and connection was upgraded to SSL/TLS,
 * 0 otherwise.
 *
 * WARNING: This is experimental code and will need to be improved.
 *
 * TODO - enable search and skip bytes before ClientHello in case it does not
 *        start at offset 0 (i.e. chello > vec_out[0].iov_base)
 * TODO - peek into more than just the current segment
 * TODO - add retry mechanism for short truncated ClientHello, possibly generic
 */
static int NONNULL(1)
protoautossl_peek_and_upgrade(pxy_conn_ctx_t *ctx)
{
	protoautossl_ctx_t *autossl_ctx = ctx->protoctx->arg;

	struct evbuffer *inbuf;
	struct evbuffer_iovec vec_out[1];
	const unsigned char *chello;

	log_finest("ENTER");

	if (OPTS_DEBUG(ctx->global)) {
		log_dbg_printf("Checking for a client hello\n");
	}

	/* peek the buffer */
	inbuf = bufferevent_get_input(ctx->src.bev);
	if (evbuffer_peek(inbuf, 1024, 0, vec_out, 1)) {
		if (ssl_tls_clienthello_parse(vec_out[0].iov_base, vec_out[0].iov_len, 0, &chello, &ctx->sslctx->sni) == 0) {
			if (OPTS_DEBUG(ctx->global)) {
				log_dbg_printf("Peek found ClientHello\n");
			}

			if (!ctx->children) {
				// This means that there was no autossl handshake prior to ClientHello, e.g. no STARTTLS message
				// This is perhaps the SSL handshake of a direct SSL connection, i.e. invalid protocol
				log_err_level(LOG_CRIT, "No children setup yet, autossl protocol error");
				return -1;
			}

			// @attention Autossl protocol should never have multiple children.
			protoautossl_upgrade_dst_child(ctx->children);

			autossl_ctx->clienthello_search = 0;
			autossl_ctx->clienthello_found = 1;
			return 1;
		} else {
			if (OPTS_DEBUG(ctx->global)) {
				log_dbg_printf("Peek found no ClientHello\n");
			}
		}
	}
	return 0;
}

static int NONNULL(1) WUNRES
protoautossl_conn_connect(pxy_conn_ctx_t *ctx)
{
	log_finest("ENTER");

	/* create server-side socket and eventbuffer */
	if (prototcp_setup_srvdst(ctx) == -1) {
		return -1;
	}
	
	// Enable srvdst r cb for autossl mode
	bufferevent_setcb(ctx->srvdst.bev, pxy_bev_readcb, NULL, pxy_bev_eventcb, ctx);
	return 0;
}

static void NONNULL(1)
protoautossl_bev_readcb_src(struct bufferevent *bev, pxy_conn_ctx_t *ctx)
{
	log_finest_va("ENTER, size=%zu", evbuffer_get_length(bufferevent_get_input(bev)));

	protoautossl_ctx_t *autossl_ctx = ctx->protoctx->arg;

#ifndef WITHOUT_USERAUTH
	if (prototcp_try_send_userauth_msg(bev, ctx)) {
		return;
	}
#endif /* !WITHOUT_USERAUTH */

	if (autossl_ctx->clienthello_search) {
		if (protoautossl_peek_and_upgrade(ctx) != 0) {
			return;
		}
	}

	if (ctx->dst.closed) {
		pxy_discard_inbuf(bev);
		return;
	}

	struct evbuffer *inbuf = bufferevent_get_input(bev);
	struct evbuffer *outbuf = bufferevent_get_output(ctx->dst.bev);

	// @todo Validate proto?

	if (!ctx->sent_sslproxy_header) {
		size_t packet_size = evbuffer_get_length(inbuf);
		// +2 for \r\n
		unsigned char *packet = pxy_malloc_packet(packet_size + ctx->sslproxy_header_len + 2, ctx);
		if (!packet) {
			return;
		}

		evbuffer_remove(inbuf, packet, packet_size);

		log_finest_va("ORIG packet, size=%zu:\n%.*s", packet_size, (int)packet_size, packet);

		pxy_insert_sslproxy_header(ctx, packet, &packet_size);
		evbuffer_add(outbuf, packet, packet_size);

		log_finest_va("NEW packet, size=%zu:\n%.*s", packet_size, (int)packet_size, packet);

		free(packet);
	}
	else {
		evbuffer_add_buffer(outbuf, inbuf);
	}
	pxy_try_set_watermark(bev, ctx, ctx->dst.bev);
}

static void NONNULL(1)
protoautossl_bev_readcb_srvdst(struct bufferevent *bev, pxy_conn_ctx_t *ctx)
{
	log_finest_va("ENTER, size=%zu", evbuffer_get_length(bufferevent_get_input(bev)));

#ifndef WITHOUT_USERAUTH
	if (prototcp_try_send_userauth_msg(ctx->src.bev, ctx)) {
		return;
	}
#endif /* !WITHOUT_USERAUTH */

	// @todo We should validate the response from the server to protect the client,
	// as we do with the smtp protocol, @see protosmtp_bev_readcb_srvdst()

	if (ctx->src.closed) {
		pxy_discard_inbuf(bev);
		return;
	}

	evbuffer_add_buffer(bufferevent_get_output(ctx->src.bev), bufferevent_get_input(bev));
	pxy_try_set_watermark(bev, ctx, ctx->src.bev);
}

static void NONNULL(1,2)
protoautossl_bev_eventcb_connected_src(UNUSED struct bufferevent *bev, UNUSED pxy_conn_ctx_t *ctx)
{
	log_finest("ENTER");
}

static int NONNULL(1)
protoautossl_enable_src(pxy_conn_ctx_t *ctx)
{
	log_finest("ENTER");

	// Create and set up tcp src.bev first
	if (prototcp_setup_src(ctx) == -1) {
		return -1;
	}

	bufferevent_setcb(ctx->src.bev, pxy_bev_readcb, pxy_bev_writecb, pxy_bev_eventcb, ctx);

	if (pxy_setup_child_listener(ctx) == -1) {
		return -1;
	}

	log_finer_va("Enabling tcp src, %s", ctx->sslproxy_header);

	bufferevent_enable(ctx->src.bev, EV_READ|EV_WRITE);
	return 0;
}

static int NONNULL(1)
protoautossl_enable_conn_src_child(pxy_conn_child_ctx_t *ctx)
{
	log_finest("ENTER");

	// Create and set up src.bev
	if (OPTS_DEBUG(ctx->conn->global)) {
		log_dbg_printf("Completing autossl upgrade\n");
	}

	// tcp src.bev was already created before
	int rv;
	if ((rv = protossl_setup_src_ssl_from_child_dst(ctx)) != 0) {
		return rv;
	}
	// Replace tcp src.bev with ssl version
	if (protossl_setup_src_new_bev_ssl_accepting(ctx->conn) == -1) {
		return -1;
	}
#if LIBEVENT_VERSION_NUMBER >= 0x02010000
	bufferevent_openssl_set_allow_dirty_shutdown(ctx->conn->src.bev, 1);
#endif /* LIBEVENT_VERSION_NUMBER >= 0x02010000 */
	bufferevent_setcb(ctx->conn->src.bev, pxy_bev_readcb, pxy_bev_writecb, pxy_bev_eventcb, ctx->conn);

	// srvdst is xferred to the first child conn, so save the ssl info for logging
	ctx->conn->sslctx->srvdst_ssl_version = strdup(SSL_get_version(ctx->dst.ssl));
	ctx->conn->sslctx->srvdst_ssl_cipher = strdup(SSL_get_cipher(ctx->dst.ssl));

	log_finer_va("Enabling ssl src, %s", ctx->conn->sslproxy_header);

	// Now open the gates for a second time after autossl upgrade
	bufferevent_enable(ctx->conn->src.bev, EV_READ|EV_WRITE);
	return 0;
}

static void NONNULL(1,2)
protoautossl_bev_eventcb_connected_dst(struct bufferevent *bev, pxy_conn_ctx_t *ctx)
{
	log_finest("ENTER");

	ctx->connected = 1;
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	bufferevent_enable(ctx->srvdst.bev, EV_READ);

	protoautossl_enable_src(ctx);
}

#ifndef WITHOUT_USERAUTH
static void NONNULL(1)
protoautossl_classify_user(pxy_conn_ctx_t *ctx)
{
	// Do not engage passthrough mode in autossl
	if (ctx->spec->opts->divertusers && !pxy_is_listuser(ctx->spec->opts->divertusers, ctx->user
#ifdef DEBUG_PROXY
			, ctx, "DivertUsers"
#endif /* DEBUG_PROXY */
			)) {
		log_fine_va("User %s not in DivertUsers; terminating connection", ctx->user);
		pxy_conn_term(ctx, 1);
	}
}
#endif /* !WITHOUT_USERAUTH */

static void NONNULL(1,2)
protoautossl_bev_eventcb_connected_srvdst(UNUSED struct bufferevent *bev, pxy_conn_ctx_t *ctx)
{
	log_finest("ENTER");

	if (prototcp_setup_dst(ctx) == -1) {
		return;
	}
	bufferevent_setcb(ctx->dst.bev, pxy_bev_readcb, pxy_bev_writecb, pxy_bev_eventcb, ctx);
	if (bufferevent_socket_connect(ctx->dst.bev, (struct sockaddr *)&ctx->spec->conn_dst_addr,
			ctx->spec->conn_dst_addrlen) == -1) {
		log_fine("FAILED bufferevent_socket_connect for dst");
		pxy_conn_term(ctx, 1);
		return;
	}

#ifndef WITHOUT_USERAUTH
	if (!ctx->term && !ctx->enomem) {
		pxy_userauth(ctx);
	}
#endif /* !WITHOUT_USERAUTH */
}

static void NONNULL(1,2)
protoautossl_bev_eventcb_connected_dst_child(struct bufferevent *bev, pxy_conn_child_ctx_t *ctx)
{
	protoautossl_ctx_t *autossl_ctx = ctx->conn->protoctx->arg;

	log_finest("ENTER");

	ctx->connected = 1;
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	bufferevent_enable(ctx->src.bev, EV_READ|EV_WRITE);

	if (autossl_ctx->clienthello_found) {
		if (protoautossl_enable_conn_src_child(ctx) != 0) {
			return;
		}

		// Check if we have arrived here right after autossl upgrade, which may be triggered by readcb on src
		// Autossl upgrade code leaves readcb without processing any data in input buffer of src
		// So, if we don't call readcb here, the connection could stall
		if (evbuffer_get_length(bufferevent_get_input(ctx->src.bev))) {
			log_finer("clienthello_found and src inbuf len > 0, calling bev_readcb for src");

			if (pxy_bev_readcb_preexec_logging_and_stats_child(bev, ctx) == -1) {
				return;
			}
			ctx->protoctx->bev_readcb(ctx->src.bev, ctx);
		}
	}
}

static void NONNULL(1)
protoautossl_bev_eventcb_src(struct bufferevent *bev, short events, pxy_conn_ctx_t *ctx)
{
	if (events & BEV_EVENT_CONNECTED) {
		protoautossl_bev_eventcb_connected_src(bev, ctx);
	} else if (events & BEV_EVENT_EOF) {
		prototcp_bev_eventcb_eof_src(bev, ctx);
	} else if (events & BEV_EVENT_ERROR) {
		prototcp_bev_eventcb_error_src(bev, ctx);
	}
}

static void NONNULL(1)
protoautossl_bev_eventcb_dst(struct bufferevent *bev, short events, pxy_conn_ctx_t *ctx)
{
	if (events & BEV_EVENT_CONNECTED) {
		protoautossl_bev_eventcb_connected_dst(bev, ctx);
	} else if (events & BEV_EVENT_EOF) {
		prototcp_bev_eventcb_eof_dst(bev, ctx);
	} else if (events & BEV_EVENT_ERROR) {
		prototcp_bev_eventcb_error_dst(bev, ctx);
	}
}

static void NONNULL(1)
protoautossl_bev_eventcb_srvdst(struct bufferevent *bev, short events, pxy_conn_ctx_t *ctx)
{
	if (events & BEV_EVENT_CONNECTED) {
		protoautossl_bev_eventcb_connected_srvdst(bev, ctx);
	} else if (events & BEV_EVENT_EOF) {
		prototcp_bev_eventcb_eof_srvdst(bev, ctx);
	} else if (events & BEV_EVENT_ERROR) {
		prototcp_bev_eventcb_error_srvdst(bev, ctx);
	}
}

static void NONNULL(1)
protoautossl_bev_eventcb_dst_child(struct bufferevent *bev, short events, pxy_conn_child_ctx_t *ctx)
{
	if (events & BEV_EVENT_CONNECTED) {
		protoautossl_bev_eventcb_connected_dst_child(bev, ctx);
	} else if (events & BEV_EVENT_EOF) {
		prototcp_bev_eventcb_eof_dst_child(bev, ctx);
	} else if (events & BEV_EVENT_ERROR) {
		prototcp_bev_eventcb_error_dst_child(bev, ctx);
	}
}

static void NONNULL(1)
protoautossl_bev_readcb(struct bufferevent *bev, void *arg)
{
	pxy_conn_ctx_t *ctx = arg;

	if (bev == ctx->src.bev) {
		protoautossl_bev_readcb_src(bev, ctx);
	} else if (bev == ctx->dst.bev) {
		prototcp_bev_readcb_dst(bev, ctx);
	} else if (bev == ctx->srvdst.bev) {
		protoautossl_bev_readcb_srvdst(bev, ctx);
	} else {
		log_err_printf("protoautossl_bev_readcb: UNKWN conn end\n");
	}
}

static void NONNULL(1)
protoautossl_bev_eventcb(struct bufferevent *bev, short events, void *arg)
{
	pxy_conn_ctx_t *ctx = arg;
	protoautossl_ctx_t *autossl_ctx = ctx->protoctx->arg;

	if ((events & BEV_EVENT_ERROR) && autossl_ctx->clienthello_found) {
		protossl_log_ssl_error(bev, ctx);
	}

	if (bev == ctx->src.bev) {
		protoautossl_bev_eventcb_src(bev, events, ctx);
	} else if (bev == ctx->dst.bev) {
		protoautossl_bev_eventcb_dst(bev, events, ctx);
	} else if (bev == ctx->srvdst.bev) {
		protoautossl_bev_eventcb_srvdst(bev, events, ctx);
	} else {
		log_err_printf("protoautossl_bev_eventcb: UNKWN conn end\n");
	}
}

static void NONNULL(1)
protoautossl_bev_eventcb_child(struct bufferevent *bev, short events, void *arg)
{
	pxy_conn_child_ctx_t *ctx = arg;

	if (bev == ctx->src.bev) {
		prototcp_bev_eventcb_src_child(bev, events, ctx);
	} else if (bev == ctx->dst.bev) {
		protoautossl_bev_eventcb_dst_child(bev, events, ctx);
	} else {
		log_err_printf("protoautossl_bev_eventcb_child: UNKWN conn end\n");
	}
}

static void NONNULL(1)
protoautossl_free(pxy_conn_ctx_t *ctx)
{
	protoautossl_ctx_t *autossl_ctx = ctx->protoctx->arg;
	free(autossl_ctx);
	protossl_free(ctx);
}

// @attention Called by thrmgr thread
protocol_t
protoautossl_setup(pxy_conn_ctx_t *ctx)
{
	ctx->protoctx->proto = PROTO_AUTOSSL;
	ctx->protoctx->connectcb = protoautossl_conn_connect;
	ctx->protoctx->init_conn = prototcp_init_conn;

	ctx->protoctx->bev_readcb = protoautossl_bev_readcb;
	ctx->protoctx->bev_writecb = prototcp_bev_writecb;
	ctx->protoctx->bev_eventcb = protoautossl_bev_eventcb;

	ctx->protoctx->proto_free = protoautossl_free;

#ifndef WITHOUT_USERAUTH
	ctx->protoctx->classify_usercb = protoautossl_classify_user;
#endif /* !WITHOUT_USERAUTH */

	ctx->protoctx->arg = malloc(sizeof(protoautossl_ctx_t));
	if (!ctx->protoctx->arg) {
		return PROTO_ERROR;
	}
	memset(ctx->protoctx->arg, 0, sizeof(protoautossl_ctx_t));
	protoautossl_ctx_t *autossl_ctx = ctx->protoctx->arg;
	autossl_ctx->clienthello_search = 1;

	ctx->sslctx = malloc(sizeof(ssl_ctx_t));
	if (!ctx->sslctx) {
		free(ctx->protoctx->arg);
		return PROTO_ERROR;
	}
	memset(ctx->sslctx, 0, sizeof(ssl_ctx_t));

	return PROTO_AUTOSSL;
}

protocol_t
protoautossl_setup_child(pxy_conn_child_ctx_t *ctx)
{
	ctx->protoctx->proto = PROTO_AUTOSSL;

	ctx->protoctx->bev_writecb = prototcp_bev_writecb_child;
	ctx->protoctx->bev_eventcb = protoautossl_bev_eventcb_child;

	return PROTO_AUTOSSL;
}

/* vim: set noet ft=c: */

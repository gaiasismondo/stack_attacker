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

#include "log.h"

#include "logger.h"
#include "sys.h"
#include "attrib.h"
#include "privsep.h"
#include "defaults.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <assert.h>
#include <sys/stat.h>
#include <netinet/in.h>

/*
 * Centralized logging code multiplexing thread access to the logger based
 * logging in separate threads.  Some log types are switchable to different
 * backends, such as syslog and stderr.
 */


/*
 * Common code for all logs.
 */
static proxy_ctx_t *proxy_ctx = NULL;

// @attention The order of names should match the numbering in LOG_DBG_MODE_* macros
char *log_dbg_mode_names[] = {
	"NONE", // = 0
	"ERROR",
	"FINE",
	"FINER",
	"FINEST",
};

void
log_exceptcb(void)
{
	log_err_level_printf(LOG_CRIT, "Error %d on logger: %s\n", errno, strerror(errno));
	/* Do not break the event loop if out of fds:
	 * Bad file descriptor (9) or Message too long (40) */
	if (errno == 9 || errno == 40) {
		return;
	}
	if (proxy_ctx) {
		proxy_loopbreak(proxy_ctx);
		proxy_ctx = NULL;
	}
}

/*
 * Error log.
 * Switchable between stderr and syslog.
 * Uses logger thread.
 */

static logger_t *err_log = NULL;
static int err_shortcut_logger = 0;
static int err_mode = LOG_ERR_MODE_STDERR;

static ssize_t
log_err_writecb(int level, UNUSED void *fh, UNUSED unsigned long ctl,
                const void *buf, size_t sz)
{
	switch (err_mode) {
		case LOG_ERR_MODE_STDERR:
			return fwrite(buf, sz - 1, 1, stderr);
		case LOG_ERR_MODE_SYSLOG:
			syslog(LOG_DAEMON|level, "%s", (const char *)buf);
			return sz;
	}
	return -1;
}

int
log_err_printf(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int rv;

	va_start(ap, fmt);
	rv = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (rv < 0)
		return -1;
	if (err_shortcut_logger) {
		return logger_write_freebuf(err_log, LOG_ERR, NULL, 0,
		                            buf, strlen(buf) + 1);
	} else {
		log_err_writecb(LOG_ERR, NULL, 0, (unsigned char*)buf, strlen(buf) + 1);
		free(buf);
	}
	return 0;
}

int
log_err_level_printf(int level, const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int rv;

	va_start(ap, fmt);
	rv = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (rv < 0)
		return -1;
	if (err_shortcut_logger) {
		return logger_write_freebuf(err_log, level, NULL, 0,
		                            buf, strlen(buf) + 1);
	} else {
		log_err_writecb(level, NULL, 0, (unsigned char*)buf, strlen(buf) + 1);
		free(buf);
	}
	return 0;
}

void
log_err_mode(int mode)
{
	err_mode = mode;
}


/*
 * Debug log.  Redirects logging to error log.
 * Switchable between error log or no logging.
 * Uses the error log logger thread.
 */

static int dbg_mode = LOG_DBG_MODE_NONE;

int
log_dbg_write_free(void *buf, size_t sz)
{
	if (dbg_mode == LOG_DBG_MODE_NONE)
		return 0;

	if (err_shortcut_logger) {
		return logger_write_freebuf(err_log, LOG_DEBUG, NULL, 0, buf, sz);
	} else {
		log_err_writecb(LOG_DEBUG, NULL, 0, buf, sz);
		free(buf);
	}
	return 0;
}

int
log_dbg_print_free(char *s)
{
	return log_dbg_write_free(s, strlen(s) + 1);
}

int
log_dbg_printf(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int rv;

	if (dbg_mode == LOG_DBG_MODE_NONE)
		return 0;

	va_start(ap, fmt);
	rv = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (rv < 0)
		return -1;
	return log_dbg_print_free(buf);
}

int
log_dbg_level_printf(int level, const char *function, int thrid, long long unsigned int id, evutil_socket_t fd, const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int rv;

	if (dbg_mode == LOG_DBG_MODE_NONE || dbg_mode < level)
		return 0;

	va_start(ap, fmt);
	rv = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (rv < 0)
		return -1;

	char *logbuf;
	if (asprintf(&logbuf, "[%s] [%d.%llu fd=%d] %s: %s\n", log_dbg_mode_names[level], thrid, id, fd, function, buf) < 0) {
		free(buf);
		return -1;
	}
	free(buf);
	return log_dbg_print_free(logbuf);
}

void
log_dbg_mode(int mode)
{
	dbg_mode = mode;
}

/*
 * Connection log.  Logs a one-liner to a file-based connection log.
 * Uses a logger thread.
 */

logger_t *connect_log = NULL;
static int connect_fd = -1;
static char *connect_fn = NULL;
static int connect_clisock = -1;

static int
log_connect_preinit(const char *logfile)
{
	connect_fd = open(logfile, O_WRONLY|O_APPEND|O_CREAT, DFLT_FILEMODE);
	if (connect_fd == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to open '%s' for writing: %s (%i)\n",
		               logfile, strerror(errno), errno);
		return -1;
	}
	connect_fn = strdup(logfile);
	if (!connect_fn) {
		close(connect_fd);
		connect_fd = -1;
		return -1;
	}
	return 0;
}

static int
log_connect_reopencb(void)
{
	close(connect_fd);
	connect_fd = privsep_client_openfile(connect_clisock,
	                                     connect_fn,
	                                     0);
	if (connect_fd == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to open '%s' for writing: %s\n",
		               connect_fn, strerror(errno));
		free(connect_fn);
		connect_fn = NULL;
		return -1;
	}
	return 0;
}

/*
 * Do the actual write to the open connection log file descriptor.
 * We prepend a timestamp here, which means that timestamps are slightly
 * delayed from the time of actual logging.  Since we only have second
 * resolution that should not make any difference.
 */
static ssize_t
log_connect_writecb(UNUSED int level, UNUSED void *fh, UNUSED unsigned long ctl,
                    const void *buf, size_t sz)
{
	char timebuf[32];
	time_t epoch;
	struct tm *utc;
	size_t n;

	time(&epoch);
	utc = gmtime(&epoch);
	n = strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S UTC ", utc);
	if (n == 0) {
		log_err_level_printf(LOG_CRIT, "Error from strftime(): buffer too small\n");
		return -1;
	}
	if ((write(connect_fd, timebuf, n) == -1) ||
	    (write(connect_fd, buf, sz) == -1)) {
		log_err_level_printf(LOG_CRIT, "Failed to write to connect log: %s\n",
		               strerror(errno));
		return -1;
	}
	return sz;
}

static void
log_connect_fini(void)
{
	close(connect_fd);
}

static int
log_info(int facility, const char *buf)
{
	size_t sz = strlen(buf) + 1;
	switch (err_mode) {
		case LOG_ERR_MODE_STDERR:
			return fwrite(buf, sz - 1, 1, stderr);
		case LOG_ERR_MODE_SYSLOG:
			syslog(facility|LOG_INFO, "%s", (const char *)buf);
			return sz;
	}
	return -1;
}

int
log_stats(const char *buf)
{
	return log_info(LOG_LOCAL0, buf);
}

int
log_conn(const char *buf)
{
	return log_info(LOG_LOCAL1, buf);
}

/*
 * Content log.
 * Logs connection content to either a single file or a directory containing
 * per-connection logs.
 * Uses a logger thread; the actual logging happens in a separate thread.
 * To ensure ordering of requests (open, write, ..., close), logging for a
 * single connection must happen from a single thread.
 * This is guaranteed by the current pxythr architecture.
 */

#define PREPFLAG_REQUEST 1
#define PREPFLAG_EOF     2

typedef struct log_content_file_ctx {
	union {
		struct {
			char *header_req;
			char *header_resp;
		} single;
		struct {
			int fd;
			char *filename;
		} dir;
		struct {
			int fd;
			char *filename;
		} spec;
	} u;
} log_content_file_ctx_t;

static int content_file_clisock = -1;
static logger_t *content_file_log = NULL;

/*
 * Split a pathname into static LHS (including final slashes) and dynamic RHS.
 * Returns -1 on error, 0 on success.
 * On success, fills in lhs and rhs with newly allocated buffers that must
 * be freed by the caller.
 */
int
log_content_split_pathspec(const char *path, char **lhs, char **rhs)
{
	const char *p, *q, *r;

	p = strchr(path, '%');
	/* at first % or EOS */

	/* skip % if next char is % (and implicitly not \0) */
	while (p && p[1] == '%') {
		p = strchr(p + 2, '%');
	}
	/* at first % that is not %%, or at EOS */

	if (!p || !p[1]) {
		/* EOS: no % that is not %% in path */
		p = path + strlen(path);
	}
	/* at first hot % or at '\0' */

	/* find last / before % */
	for (r = q = strchr(path, '/'); q && (q < p); q = strchr(q + 1, '/')) {
		r = q;
	}
	if (!(p = r)) {
		/* no / found, use dummy ./ as LHS */
		*lhs = strdup("./");
		if (!*lhs)
			return -1;
		*rhs = strdup(path);
		if (!*rhs) {
			free(*lhs);
			return -1;
		}
		return 0;
	}
	/* at last / terminating the static part of path */

	p++; /* skip / */
	*lhs = malloc(p - path + 1 /* for terminating null */);
	if (!*lhs)
		return -1;
	memcpy(*lhs, path, p - path);
	(*lhs)[p - path] = '\0';
	*rhs = strdup(p);
	if (!*rhs) {
		free(*lhs);
		return -1;
	}

	return 0;
}

/*
 * Generate a log path based on the given log spec.
 * Returns an allocated buffer which must be freed by caller, or NULL on error.
 */
#define PATH_BUF_INC	1024
static char * MALLOC NONNULL(1,2,3)
log_content_format_pathspec(const char *logspec,
                            char *srchost, char *srcport,
                            char *dsthost, char *dstport,
                            char *exec_path, char *user, char *group)
{
	/* set up buffer to hold our generated file path */
	size_t path_buflen = PATH_BUF_INC;
	char *path_buf = malloc(path_buflen);
	if (path_buf == NULL) {
		log_err_level_printf(LOG_CRIT, "Failed to allocate path buffer\n");
		return NULL;
	}

	/* initialize the buffer as an empty C string */
	path_buf[0] = '\0';

	/* iterate over format specifiers */
	size_t path_len = 0;
	for (const char *p = logspec; *p != '\0'; p++) {
		const char *elem = NULL;
		size_t elem_len = 0;

		const char iso8601[] =  "%Y%m%dT%H%M%SZ";
		char timebuf[24]; /* sized for ISO 8601 format */
		char addrbuf[INET6_ADDRSTRLEN + 8]; /* [host]:port */

		/* parse the format string and generate the next path element */
		switch (*p) {
		case '%':
			p++;
			/* handle format specifiers. */
			switch (*p) {
			case '\0':
				/* unexpected eof; backtrack and discard
				 * invalid format spec */
				p--;
				elem_len = 0;
				break;
			case '%':
				elem = p;
				elem_len = 1;
				break;
			case 'd':
				if (snprintf(addrbuf, sizeof(addrbuf),
				             "%s,%s", dsthost, dstport) < 0) {
					addrbuf[0] = '?';
					addrbuf[1] = '\0';
				}
				elem = addrbuf;
				elem_len = strlen(addrbuf);
				break;
			case 'D':
				elem = dsthost;
				elem_len = strlen(dsthost);
				break;
			case 'p':
				elem = dstport;
				elem_len = strlen(dstport);
				break;
			case 's':
				if (snprintf(addrbuf, sizeof(addrbuf),
				             "%s,%s", srchost, srcport) < 0) {
					addrbuf[0] = '?';
					addrbuf[1] = '\0';
				}
				elem = addrbuf;
				elem_len = strlen(addrbuf);
				break;
			case 'S':
				elem = srchost;
				elem_len = strlen(srchost);
				break;
			case 'q':
				elem = srcport;
				elem_len = strlen(srcport);
				break;
			case 'x':
				if (exec_path) {
					char *match = exec_path;
					while ((match = strchr(match, '/')) != NULL) {
						match++;
						elem = match;
					}
					elem_len = elem ? strlen(elem) : 0;
				} else {
					elem_len = 0;
				}
				break;
			case 'X':
				elem = exec_path;
				elem_len = exec_path ? strlen(exec_path) : 0;
				break;
			case 'u':
				elem = user;
				elem_len = user ? strlen(user) : 0;
				break;
			case 'g':
				elem = group;
				elem_len = group ? strlen(group) : 0;
				break;
			case 'T': {
				time_t epoch;
				struct tm *utc;

				time(&epoch);
				utc = gmtime(&epoch);
				strftime(timebuf, sizeof(timebuf), iso8601, utc);

				elem = timebuf;
				elem_len = sizeof(timebuf);
				break;
			}}
			break;
		default:
			elem = p;
			elem_len = 1;
			break;
		}

		if (elem_len > 0) {
			/* growing the buffer to fit elem_len + terminating \0 */
			if (path_buflen - path_len < elem_len + 1) {
				/* Grow in PATH_BUF_INC chunks.
				 * Note that the use of `PATH_BUF_INC' provides
				 * our guaranteed space for a trailing '\0' */
				path_buflen += elem_len + PATH_BUF_INC;
				char *newbuf = realloc(path_buf, path_buflen);
				if (newbuf == NULL) {
					log_err_level_printf(LOG_CRIT, "Failed to reallocate"
					               " path buffer\n");
					free(path_buf);
					return NULL;
				}
				path_buf = newbuf;
			}

			strncat(path_buf, elem, elem_len);
			path_len += elem_len;
		}
	}

	/* apply terminating NUL */
	assert(path_buflen > path_len);
	path_buf[path_len] = '\0';
	return path_buf;
}
#undef PATH_BUF_INC

/*
 * log_content_ctx_t is preallocated by the caller (part of connection ctx).
 */
int
log_content_open(log_content_ctx_t *ctx, opts_t *opts,
                 char *srchost, char *srcport,
                 char *dsthost, char *dstport,
                 char *exec_path, char *user, char *group)
{
	char timebuf[24];
	time_t epoch;
	struct tm *utc;
	char *dsthost_clean = NULL;
	char *srchost_clean = NULL;

	if (ctx->file)
		return 0; /* does this actually happen? */

	if (opts->contentlog_isdir || opts->contentlog_isspec) {
		if (opts->contentlog_isdir) {
			if (time(&epoch) == -1) {
				log_err_level_printf(LOG_CRIT, "Failed to get time\n");
				goto errout;
			}
			if ((utc = gmtime(&epoch)) == NULL) {
				log_err_level_printf(LOG_CRIT, "Failed to convert time:"
				               " %s (%i)\n",
				               strerror(errno), errno);
				goto errout;
			}
			if (!strftime(timebuf, sizeof(timebuf),
			              "%Y%m%dT%H%M%SZ", utc)) {
				log_err_level_printf(LOG_CRIT, "Failed to format time:"
				               " %s (%i)\n",
				               strerror(errno), errno);
				goto errout;
			}
		}

		srchost_clean = sys_ip46str_sanitize(srchost);
		if (!srchost_clean) {
			log_err_level_printf(LOG_CRIT, "Failed to sanitize srchost\n");
			goto errout;
		}
		dsthost_clean = sys_ip46str_sanitize(dsthost);
		if (!dsthost_clean) {
			log_err_level_printf(LOG_CRIT, "Failed to sanitize dsthost\n");
			free(srchost_clean);
			goto errout;
		}
	}

	if (opts->contentlog) {
		ctx->file = malloc(sizeof(log_content_file_ctx_t));
		if (!ctx->file)
			goto errout;
		memset(ctx->file, 0, sizeof(log_content_file_ctx_t));

		if (opts->contentlog_isdir) {
			/* per-connection-file content log (-S) */
			if (asprintf(&ctx->file->u.dir.filename,
			             "%s/%s-%s,%s-%s,%s.log",
			             opts->contentlog, timebuf,
			             srchost_clean, srcport,
			             dsthost_clean, dstport) < 0) {
				log_err_level_printf(LOG_CRIT, "Failed to format filename:"
				               " %s (%i)\n",
				               strerror(errno), errno);
				goto errout;
			}
		} else if (opts->contentlog_isspec) {
			/* per-connection-file content log with logspec (-F) */
			ctx->file->u.spec.filename =
				log_content_format_pathspec(opts->contentlog,
				                            srchost_clean,
				                            srcport,
				                            dsthost_clean,
				                            dstport,
				                            exec_path,
				                            user, group);
			if (!ctx->file->u.spec.filename) {
				goto errout;
			}
		} else {
			/* single-file content log (-L) */
			if (asprintf(&ctx->file->u.single.header_req,
			             "[%s]:%s -> [%s]:%s",
			             srchost, srcport, dsthost, dstport) < 0) {
				goto errout;
			}
			if (asprintf(&ctx->file->u.single.header_resp,
			             "[%s]:%s -> [%s]:%s",
			             dsthost, dstport, srchost, srcport) < 0) {
				free(ctx->file->u.single.header_req);
				goto errout;
			}
		}
	}

	/* submit open events */
	if (ctx->file) {
		if (logger_open(content_file_log, ctx->file) == -1)
			goto errout;
	}

	if (srchost_clean)
		free(srchost_clean);
	if (dsthost_clean)
		free(dsthost_clean);
	return 0;

errout:
	if (srchost_clean)
		free(srchost_clean);
	if (dsthost_clean)
		free(dsthost_clean);
	if (ctx->file)
		free(ctx->file);
	memset(ctx, 0, sizeof(log_content_ctx_t));
	return -1;
}

/*
 * On failure, lb is not freed.
 */
int
log_content_submit(log_content_ctx_t *ctx, logbuf_t *lb, int is_request)
{
	unsigned long prepflags = 0;

	if (is_request)
		prepflags |= PREPFLAG_REQUEST;

	lb = logbuf_make_contiguous(lb);
	if (!lb)
		return -1;

	if (content_file_log) {
		if (logger_submit(content_file_log, ctx->file,
		                  prepflags, lb) == -1) {
			return -1;
		}
	}
	return 0;
}

int
log_content_close(log_content_ctx_t *ctx, int by_requestor)
{
	unsigned long prepflags = PREPFLAG_EOF;
	unsigned long ctl;

	if (by_requestor) {
		prepflags |= PREPFLAG_REQUEST;
		ctl = LBFLAG_IS_REQ;
	} else {
		ctl = LBFLAG_IS_RESP;
	}

	/* We call submit an empty log buffer in order to give the content log
	 * a chance to insert an EOF footer to be logged before actually
	 * closing the file.  The logger_close() call will actually close the
	 * log.  Some logs prefer to use the close callback for logging the
	 * close event to the log. */
	if (content_file_log && ctx->file) {
		if (logger_submit(content_file_log, ctx->file,
		                  prepflags, NULL) == -1) {
			return -1;
		}
		if (logger_close(content_file_log, ctx->file, ctl) == -1) {
			return -1;
		}
		ctx->file = NULL;
	}
	return 0;
}

/*
 * Log-type specific code.
 *
 * The init/fini functions are executed globally in the main thread.
 * Callback functions are executed in the logger thread.
 */

static int
log_content_file_dir_opencb(void *fh)
{
	log_content_file_ctx_t *ctx = fh;

	if ((ctx->u.dir.fd = privsep_client_openfile(content_file_clisock,
	                                             ctx->u.dir.filename,
	                                             0)) == -1) {
		log_err_level_printf(LOG_CRIT, "Opening logdir file '%s' failed: %s (%i)\n",
		               ctx->u.dir.filename,
		               strerror(errno), errno);
		return -1;
	}
	return 0;
}

static void
log_content_file_dir_closecb(void *fh, UNUSED unsigned long ctl)
{
	log_content_file_ctx_t *ctx = fh;

	if (ctx->u.dir.filename)
		free(ctx->u.dir.filename);
	if (ctx->u.dir.fd != 1)
		close(ctx->u.dir.fd);
	free(ctx);
}

static ssize_t
log_content_file_dir_writecb(UNUSED int level, void *fh, UNUSED unsigned long ctl,
                             const void *buf, size_t sz)
{
	log_content_file_ctx_t *ctx = fh;

	if (write(ctx->u.dir.fd, buf, sz) == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to write to content log: %s\n",
		               strerror(errno));
		return -1;
	}
	return sz;
}

static int
log_content_file_spec_opencb(void *fh)
{
	log_content_file_ctx_t *ctx = fh;

	if ((ctx->u.spec.fd = privsep_client_openfile(content_file_clisock,
	                                              ctx->u.spec.filename,
	                                              1)) == -1) {
		log_err_level_printf(LOG_CRIT, "Opening logspec file '%s' failed: %s (%i)\n",
		               ctx->u.spec.filename, strerror(errno), errno);
		return -1;
	}
	return 0;
}

static void
log_content_file_spec_closecb(void *fh, UNUSED unsigned long ctl)
{
	log_content_file_ctx_t *ctx = fh;

	if (ctx->u.spec.filename)
		free(ctx->u.spec.filename);
	if (ctx->u.spec.fd != -1)
		close(ctx->u.spec.fd);
	free(ctx);
}

static ssize_t
log_content_file_spec_writecb(UNUSED int level, void *fh, UNUSED unsigned long ctl,
                              const void *buf, size_t sz)
{
	log_content_file_ctx_t *ctx = fh;

	if (write(ctx->u.spec.fd, buf, sz) == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to write to content log: %s\n",
		               strerror(errno));
		return -1;
	}
	return sz;
}

static int content_file_single_fd = -1;
static char *content_file_single_fn = NULL;

static int
log_content_file_single_preinit(const char *logfile)
{
	content_file_single_fd = open(logfile, O_WRONLY|O_APPEND|O_CREAT,
	                       DFLT_FILEMODE);
	if (content_file_single_fd == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to open '%s' for writing: %s (%i)\n",
		               logfile, strerror(errno), errno);
		return -1;
	}
	content_file_single_fn = strdup(logfile);
	if (!content_file_single_fn) {
		close(content_file_single_fd);
		content_file_single_fd = -1;
		return -1;
	}
	return 0;
}

static void
log_content_file_single_fini(void)
{
	if (content_file_single_fn) {
		free(content_file_single_fn);
		content_file_single_fn = NULL;
	}
	if (content_file_single_fd != -1) {
		close(content_file_single_fd);
		content_file_single_fd = -1;
	}
}

static int
log_content_file_single_reopencb(void)
{
	close(content_file_single_fd);
	content_file_single_fd = privsep_client_openfile(content_file_clisock,
	                                                 content_file_single_fn,
	                                                 0);
	if (content_file_single_fd == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to open '%s' for writing: %s (%i)\n",
		               content_file_single_fn, strerror(errno), errno);
		return -1;
	}
	return 0;
}

static void
log_content_file_single_closecb(void *fh, UNUSED unsigned long ctl)
{
	log_content_file_ctx_t *ctx = fh;

	if (ctx->u.single.header_req) {
		free(ctx->u.single.header_req);
	}
	if (ctx->u.single.header_resp) {
		free(ctx->u.single.header_resp);
	}
	free(ctx);
}

static ssize_t
log_content_file_single_writecb(UNUSED int level, void *fh, UNUSED unsigned long ctl,
                                const void *buf, size_t sz)
{
	UNUSED log_content_file_ctx_t *ctx = fh;

	if (write(content_file_single_fd, buf, sz) == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to write to content log: %s\n",
		               strerror(errno));
		return -1;
	}
	return sz;
}

static logbuf_t *
log_content_file_single_prepcb(void *fh, unsigned long prepflags,
                               logbuf_t *lb)
{
	log_content_file_ctx_t *ctx = fh;
	int is_request = !!(prepflags & PREPFLAG_REQUEST);
	logbuf_t *head;
	time_t epoch;
	struct tm *utc;
	char *header;

	if (!(header = is_request ? ctx->u.single.header_req
	                          : ctx->u.single.header_resp))
		goto out;

	/* prepend size tag or EOF, and newline */
	if (prepflags & PREPFLAG_EOF) {
		head = logbuf_new_printf(NULL, " (EOF)\n");
	} else {
		head = logbuf_new_printf(lb, " (%zu):\n", logbuf_size(lb));
	}
	if (!head) {
		log_err_level_printf(LOG_CRIT, "Failed to allocate memory\n");
		logbuf_free(lb);
		return NULL;
	}
	lb = head;

	/* prepend header */
	head = logbuf_new_copy(header, strlen(header), lb);
	if (!head) {
		log_err_level_printf(LOG_CRIT, "Failed to allocate memory\n");
		logbuf_free(lb);
		return NULL;
	}
	lb = head;

	/* prepend timestamp */
	head = logbuf_new_alloc(32, lb);
	if (!head) {
		log_err_level_printf(LOG_CRIT, "Failed to allocate memory\n");
		logbuf_free(lb);
		return NULL;
	}
	lb = head;
	time(&epoch);
	utc = gmtime(&epoch);
	lb->sz = strftime((char*)lb->buf, lb->sz, "%Y-%m-%d %H:%M:%S UTC ",
	                  utc);

out:
	return lb;
}

/*
 * Initialization and destruction.
 */

/*
 * Log pre-init: open all log files but don't start any threads, since we may
 * fork() after pre-initialization.
 * Return -1 on errors, 0 otherwise.
 */
int
log_preinit(opts_t *opts)
{
	logger_reopen_func_t reopencb;
	logger_open_func_t opencb;
	logger_close_func_t closecb;
	logger_write_func_t writecb;
	logger_prep_func_t prepcb;

	if (opts->contentlog) {
		if (opts->contentlog_isdir) {
			reopencb = NULL;
			opencb = log_content_file_dir_opencb;
			closecb = log_content_file_dir_closecb;
			writecb = log_content_file_dir_writecb;
			prepcb = NULL;
		} else if (opts->contentlog_isspec) {
			reopencb = NULL;
			opencb = log_content_file_spec_opencb;
			closecb = log_content_file_spec_closecb;
			writecb = log_content_file_spec_writecb;
			prepcb = NULL;
		} else {
			if (log_content_file_single_preinit(opts->contentlog) == -1)
				goto out;
			reopencb = log_content_file_single_reopencb;
			opencb = NULL;
			closecb = log_content_file_single_closecb;
			writecb = log_content_file_single_writecb;
			prepcb = log_content_file_single_prepcb;
		}
		if (!(content_file_log = logger_new(reopencb, opencb, closecb,
		                                    writecb, prepcb,
		                                    log_exceptcb))) {
			log_content_file_single_fini();
			goto out;
		}
	}
	if (opts->connectlog) {
		if (log_connect_preinit(opts->connectlog) == -1)
			goto out;
		if (!(connect_log = logger_new(log_connect_reopencb,
		                               NULL, NULL,
		                               log_connect_writecb, NULL,
		                               log_exceptcb))) {
			log_connect_fini();
			goto out;
		}
	}
	if (!(err_log = logger_new(NULL, NULL, NULL, log_err_writecb, NULL,
	                           log_exceptcb)))
		goto out;
	return 0;

out:
	if (connect_log) {
		log_connect_fini();
		logger_free(connect_log);
	}
	if (content_file_log) {
		log_content_file_single_fini();
		logger_free(content_file_log);
	}
	return -1;
}

/*
 * Close all file descriptors opened by log_preinit; used in privsep parent.
 * Only undo content, connect and masterkey logs, leave error and debug log
 * functional.
 */
void
log_preinit_undo(void)
{
	if (connect_log) {
		log_connect_fini();
		logger_free(connect_log);
	}
	if (content_file_log) {
		log_content_file_single_fini();
		logger_free(content_file_log);
	}
}

/*
 * Log post-init: start logging threads.
 * Return -1 on errors, 0 otherwise.
 */
int
log_init(opts_t *opts, proxy_ctx_t *ctx, int clisock[5])
{
	proxy_ctx = ctx;
	if (err_log)
		if (logger_start(err_log) == -1)
			return -1;
	if (!opts->debug) {
		err_shortcut_logger = 1;
	}

	if (connect_log) {
		connect_clisock = clisock[1];
		if (logger_start(connect_log) == -1)
			return -1;
	} else {
		privsep_client_close(clisock[1]);
	}

	if (content_file_log) {
		content_file_clisock = clisock[2];
		if (logger_start(content_file_log) == -1)
			return -1;
	} else {
		privsep_client_close(clisock[2]);
	}

	return 0;
}

/*
 * Drain and cleanup.  Tell all loggers to leave, then join all logger threads,
 * and finally free resources and close log files.
 */
void
log_fini(void)
{
	/* switch back to direct logging so we can still log errors while
	 * tearing down the logging infrastructure */
	err_shortcut_logger = 1;

	if (content_file_log)
		logger_leave(content_file_log);
	if (connect_log)
		logger_leave(connect_log);
	if (err_log)
		logger_leave(err_log);

	if (content_file_log)
		logger_join(content_file_log);
	if (connect_log)
		logger_join(connect_log);
	if (err_log)
		logger_join(err_log);

	if (content_file_log)
		logger_free(content_file_log);
	if (connect_log)
		logger_free(connect_log);
	if (err_log)
		logger_free(err_log);

	if (content_file_log)
		log_content_file_single_fini();
	if (connect_log)
		log_connect_fini();

	if (content_file_clisock != -1)
		privsep_client_close(content_file_clisock);
	if (connect_clisock != -1)
		privsep_client_close(connect_clisock);
}

int
log_reopen(void)
{
	int rv = 0;

	if (content_file_log)
		if (logger_reopen(content_file_log) == -1)
			rv = -1;
	if (connect_log)
		if (logger_reopen(connect_log) == -1)
			rv = -1;

	return rv;
}

/* vim: set noet ft=c: */

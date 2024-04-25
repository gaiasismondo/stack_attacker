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

/* silence daemon(3) deprecation warning on Mac OS X */
#if __APPLE__
#define daemon xdaemon
#endif /* __APPLE__ */

#include "opts.h"
#include "proxy.h"
#include "privsep.h"
#include "ssl.h"
#include "nat.h"
#include "proc.h"
#include "cachemgr.h"
#include "sys.h"
#include "log.h"
#include "build.h"
#include "defaults.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#ifndef __BSD__
#include <getopt.h>
#endif /* !__BSD__ */

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <event2/event.h>
#ifndef WITHOUT_MIRROR
#include <libnet.h>
#include <pcap.h>
#endif /* !WITHOUT_MIRROR */

#if __APPLE__
#undef daemon
extern int daemon(int, int);
#endif /* __APPLE__ */

/*
 * Print version information to stderr.
 */
static void
main_version(void)
{
	fprintf(stderr, "%s %s (built %s)\n",
	                PKGLABEL, build_version, build_date);
	if (strlen(build_version) < 5) {
		/*
		 * Note to package maintainers:  If you break the version
		 * string in your build, it will be impossible to provide
		 * proper upstream support to the users of the package,
		 * because it will be difficult or impossible to identify
		 * the exact codebase that is being used by the user
		 * reporting a bug.  The version string is provided through
		 * different means depending on whether the code is a git
		 * checkout, a tarball downloaded from GitHub or a release.
		 * See GNUmakefile for the gory details.
		 */
		fprintf(stderr, "---------------------------------------"
		                "---------------------------------------\n");
		fprintf(stderr, "WARNING: Something is wrong with the "
		                "version compiled into sslproxy!\n");
		fprintf(stderr, "The version should contain a release "
		                "number and/or a git commit reference.\n");
		fprintf(stderr, "If using a package, please report a bug "
		                "to the distro package maintainer.\n");
		fprintf(stderr, "---------------------------------------"
		                "---------------------------------------\n");
	}
	fprintf(stderr, "Copyright (c) 2017-2021, Soner Tari <sonertari@gmail.com>\n");
	fprintf(stderr, "https://github.com/sonertari/SSLproxy\n");
	fprintf(stderr, "Copyright (c) 2009-2019, "
	                "Daniel Roethlisberger <daniel@roe.ch>\n");
	fprintf(stderr, "https://www.roe.ch/SSLsplit\n");
	if (build_info[0]) {
		fprintf(stderr, "Build info: %s\n", build_info);
	}
	if (build_features[0]) {
		fprintf(stderr, "Features: %s\n", build_features);
	}
	nat_version();
	fprintf(stderr, "Local process info support: ");
#ifdef HAVE_LOCAL_PROCINFO
	fprintf(stderr, "yes (" LOCAL_PROCINFO_STR ")\n");
#else /* !HAVE_LOCAL_PROCINFO */
	fprintf(stderr, "no\n");
#endif /* !HAVE_LOCAL_PROCINFO */
	ssl_openssl_version();
	fprintf(stderr, "compiled against libevent %s\n", LIBEVENT_VERSION);
	fprintf(stderr, "rtlinked against libevent %s\n", event_get_version());
#ifndef WITHOUT_MIRROR
	fprintf(stderr, "compiled against libnet %s\n", LIBNET_VERSION);
#ifndef __OpenBSD__
	const char *lnv = libnet_version();
	if (!strncmp(lnv, "libnet version ", 15))
		lnv += 15;
	fprintf(stderr, "rtlinked against libnet %s\n", lnv);
#else /* __OpenBSD__ */
	fprintf(stderr, "rtlinked against libnet n/a\n");
#endif /* __OpenBSD__ */
	fprintf(stderr, "compiled against libpcap n/a\n");
	const char *lpv = pcap_lib_version();
	if (!strncmp(lpv, "libpcap version ", 16))
		lpv += 16;
	fprintf(stderr, "rtlinked against libpcap %s\n", lpv);
#endif /* !WITHOUT_MIRROR */
#ifndef WITHOUT_USERAUTH
	fprintf(stderr, "compiled against sqlite %s\n", SQLITE_VERSION);
	fprintf(stderr, "rtlinked against sqlite %s\n", sqlite3_libversion());
#endif /* !WITHOUT_USERAUTH */
	fprintf(stderr, "%d CPU cores detected\n", sys_get_cpu_cores());
}

/*
 * Print usage to stderr.
 */
static void
main_usage(void)
{
	const char *dflt, *warn;
	const char *usagefmt1 =
"Usage: %s [-D] [-f conffile] [-o opt=val] [options...] [proxyspecs...]\n"
"  -f conffile use conffile to load configuration from\n"
"  -o opt=val  override conffile option opt with value val\n"
"  -c pemfile  use CA cert (and key) from pemfile to sign forged certs\n"
"  -k pemfile  use CA key (and cert) from pemfile to sign forged certs\n"
"  -C pemfile  use CA chain from pemfile (intermediate and root CA certs)\n"
"  -K pemfile  use key from pemfile for leaf certs (default: generate)\n"
"  -q crlurl   use URL as CRL distribution point for all forged certs\n"
"  -t certdir  use cert+chain+key PEM files from certdir to target all sites\n"
"              matching the common names (non-matching: -T or generate if CA)\n"
"  -A pemfile  use cert+chain+key PEM file as fallback leaf cert when none of\n"
"              those given by -t match, instead of generating one on the fly\n"
"  -w gendir   write leaf key and only generated certificates to gendir\n"
"  -W gendir   write leaf key and all certificates to gendir\n"
"  -O          deny all OCSP requests on all proxyspecs\n"
"  -P          passthrough SSL connections if they cannot be split because of\n"
"              client cert auth or no matching cert and no CA (default: drop)\n"
"  -a pemfile  use cert from pemfile when destination requests client certs\n"
"  -b pemfile  use key from pemfile when destination requests client certs\n"
#ifndef OPENSSL_NO_DH
"  -g pemfile  use DH group params from pemfile (default: keyfiles or auto)\n"
#define OPT_g "g:"
#else /* OPENSSL_NO_DH */
#define OPT_g 
#endif /* !OPENSSL_NO_DH */
#ifndef OPENSSL_NO_ECDH
"  -G curve    use ECDH named curve (default: " DFLT_CURVE ")\n"
#define OPT_G "G:"
#else /* OPENSSL_NO_ECDH */
#define OPT_G 
#endif /* OPENSSL_NO_ECDH */
#ifdef SSL_OP_NO_COMPRESSION
"  -Z          disable SSL/TLS compression on all connections\n"
#define OPT_Z "Z"
#else /* !SSL_OP_NO_COMPRESSION */
#define OPT_Z 
#endif /* !SSL_OP_NO_COMPRESSION */
"  -r proto    only support one of " SSL_PROTO_SUPPORT_S "(default: all)\n"
"  -R proto    disable one of " SSL_PROTO_SUPPORT_S "(default: none)\n"
"  -s ciphers  use the given OpenSSL ciphers spec (default: " DFLT_CIPHERS ")\n"
"  -U ciphersuites use the given OpenSSL ciphersuites spec with TLS 1.3\n"
"  (default: " DFLT_CIPHERSUITES ")\n"
#ifndef OPENSSL_NO_ENGINE
"  -x engine   load OpenSSL engine with the given identifier\n"
#define OPT_x "x:"
#else /* OPENSSL_NO_ENGINE */
#define OPT_x 
#endif /* OPENSSL_NO_ENGINE */
"  -e engine   specify default NAT engine to use (default: %s)\n"
"  -E          list available NAT engines and exit\n"
"  -u user     drop privileges to user (default if run as root: " DFLT_DROPUSER ")\n"
"  -m group    when using -u, override group (default: primary group of user)\n"
"  -j jaildir  chroot() to jaildir (impacts sni proxyspecs, see manual page)\n"
"  -p pidfile  write pid to pidfile (default: no pid file)\n"
"  -l logfile  connect log: log one line summary per connection to logfile\n"
"  -J          enable connection statistics logging\n"
"  -L logfile  content log: full data to file or named pipe (excludes -S/-F)\n"
"  -S logdir   content log: full data to separate files in dir (excludes -L/-F)\n"
"  -F pathspec content log: full data to sep files with %% subst (excl. -L/-S):\n"
"              %%T - initial connection time as an ISO 8601 UTC timestamp\n"
"              %%d - destination host and port\n"
"              %%D - destination host\n"
"              %%p - destination port\n"
"              %%s - source host and port\n"
"              %%S - source host\n"
"              %%q - source port\n"
#ifdef HAVE_LOCAL_PROCINFO
"              %%x - base name of local process        (requires -i)\n"
"              %%X - full path to local process        (requires -i)\n"
"              %%u - user name or id of local process  (requires -i)\n"
"              %%g - group name or id of local process (requires -i)\n"
#endif /* HAVE_LOCAL_PROCINFO */
"              %%%% - literal '%%'\n"
#ifdef HAVE_LOCAL_PROCINFO
"      e.g.    \"/var/log/sslproxy/%%X/%%u-%%s-%%d-%%T.log\"\n"
#else /* !HAVE_LOCAL_PROCINFO */
"      e.g.    \"/var/log/sslproxy/%%T-%%s-%%d.log\"\n"
#endif /* HAVE_LOCAL_PROCINFO */
"  -X pcapfile pcap log: packets to pcapfile (excludes -Y/-y)\n"
"  -Y pcapdir  pcap log: packets to separate files in dir (excludes -X/-y)\n"
"  -y pathspec pcap log: packets to sep files with %% subst (excl. -X/-Y):\n"
"              see option -F for pathspec format\n"
#ifndef WITHOUT_MIRROR
"  -I if       mirror packets to interface\n"
"  -T addr     mirror packets to target address (optionally used with -I)\n"
#define OPT_I "I:"
#define OPT_T "T:"
#else /* WITHOUT_MIRROR */
#define OPT_I 
#define OPT_T 
#endif /* WITHOUT_MIRROR */
"  -M logfile  log master keys to logfile in SSLKEYLOGFILE format\n"
#ifdef HAVE_LOCAL_PROCINFO
"  -i          look up local process owning each connection for logging\n"
#define OPT_i "i"
#else /* !HAVE_LOCAL_PROCINFO */
#define OPT_i 
#endif /* HAVE_LOCAL_PROCINFO */
"  -d          daemon mode: run in background, log error messages to syslog\n"
"  -D          debug mode: run in foreground, log debug messages on stderr\n"
"  -V          print version information and exit\n"
"  -h          print usage information and exit\n";
	const char *usagefmt2 =
"  proxyspec = type listenaddr+port  \"up:\"port [\"ua:\"utmaddr \"ra:\"returnaddr]\n"
"                [natengine|targetaddr+port|\"sni\"+port]\n"
"      e.g.    http 0.0.0.0 8080 up:8080 roe.ch 80    # http/4; static\n"
"                                                     # hostname dst\n"
"              https ::1 8443 up:8080 2001:db8::1 443 # https/6; static\n"
"                                                     # address dst\n"
"              https 127.0.0.1 9443 up:8080 sni 443   # https/4; SNI DNS\n"
"                                                     # lookups\n"
"              tcp 127.0.0.1 10025 up:9199            # tcp/4; default NAT\n"
"                                                     # engine\n"
"              ssl 2001:db8::2 9999 up:19999 pf       # ssl/6; NAT engine 'pf'\n"
"              autossl ::1 10025 up:9199              # autossl/6; STARTTLS\n"
"                                                     # et al\n"
"Example:\n"
"  %s -k ca.key -c ca.pem -P  https 127.0.0.1 8443  up:8080\n"
"%s";

	if (!(dflt = nat_getdefaultname())) {
		dflt = "n/a";
		warn = "\nWarning: no supported NAT engine on this platform!\n"
		       "Only static and SNI proxyspecs are supported.\n";
	} else {
		warn = "";
	}

	fprintf(stderr, usagefmt1, build_pkgname, dflt);
	fprintf(stderr, usagefmt2, build_pkgname, warn);
}

/*
 * Callback to load a cert/chain/key combo from a single PEM file for -t.
 * A return value of -1 indicates a fatal error to the file walker.
 */
static int
main_load_leafcert(const char *filename, void *arg)
{
	global_t *global = arg;
	cert_t *cert;
	char **names;

	cert = opts_load_cert_chain_key(filename);
	if (!cert)
		return -1;

	if (OPTS_DEBUG(global)) {
		log_dbg_printf("Targets for '%s':", filename);
	}
	names = ssl_x509_names(cert->crt);
	for (char **p = names; *p; p++) {
		/* be deliberately vulnerable to NULL prefix attacks */
		char *sep;
		if ((sep = strchr(*p, '!'))) {
			*sep = '\0';
		}
		if (OPTS_DEBUG(global)) {
			log_dbg_printf(" '%s'", *p);
		}
		cachemgr_tgcrt_set(*p, cert);
		free(*p);
	}
	if (OPTS_DEBUG(global)) {
		log_dbg_printf("\n");
	}
	free(names);
	cert_free(cert);
	return 0;
}

static void
main_check_opts(opts_t *opts, const char *argv0)
{
	if (opts->cacrt && !opts->cakey) {
		fprintf(stderr, "%s: no CA key specified (-k).\n",
						argv0);
		exit(EXIT_FAILURE);
	}
	if (opts->cakey && !opts->cacrt) {
		fprintf(stderr, "%s: no CA cert specified (-c).\n",
						argv0);
		exit(EXIT_FAILURE);
	}
	if (opts->cakey && opts->cacrt &&
		(X509_check_private_key(opts->cacrt, opts->cakey) != 1)) {
		fprintf(stderr, "%s: CA cert does not match key.\n",
						argv0);
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
	if (!opts->cakey &&
	    !opts->global->leafcertdir &&
	    !opts->global->defaultleafcert) {
		fprintf(stderr, "%s: at least one of -c/-k, -t or -A "
		                "must be specified\n", argv0);
		exit(EXIT_FAILURE);
	}
}

/*
 * Main entry point.
 */
int
main(int argc, char *argv[])
{
	const char *argv0;
	int ch;
	global_t *global;
	char *natengine;
	int pidfd = -1;
	int rv = EXIT_FAILURE;

	argv0 = argv[0];
	global = global_new();
	if (nat_getdefaultname()) {
		natengine = strdup(nat_getdefaultname());
		if (!natengine)
			oom_die(argv0);
	} else {
		natengine = NULL;
	}

	// This var is temporary, hence freed immediately after configuration is complete.
	global_opts_str_t *global_opts_str = malloc(sizeof(global_opts_str_t));
	memset(global_opts_str, 0, sizeof(global_opts_str_t));

	while ((ch = getopt(argc, argv,
	                    OPT_g OPT_G OPT_Z OPT_i OPT_x OPT_T OPT_I
	                    "k:c:C:K:t:A:OPa:b:s:U:r:R:e:Eu:m:j:p:l:L:S:F:M:"
	                    "dD::VhW:w:q:f:o:X:Y:y:J")) != -1) {
		switch (ch) {
			case 'f':
				if (global->conffile)
					free(global->conffile);
				global->conffile = strdup(optarg);
				if (!global->conffile)
					oom_die(argv0);
#ifdef DEBUG_OPTS
				log_dbg_printf("Conf file: %s\n", global->conffile);
#endif /* DEBUG_OPTS */
				if (global_load_conffile(global, argv0, &natengine, global_opts_str) == -1) {
					exit(EXIT_FAILURE);
				}
				break;
			case 'o':
				if (global_set_option(global, argv0, optarg, &natengine, global_opts_str) == -1) {
					exit(EXIT_FAILURE);
				}
				break;
			case 'c':
				opts_set_cacrt(global->opts, argv0, optarg, global_opts_str);
				break;
			case 'k':
				opts_set_cakey(global->opts, argv0, optarg, global_opts_str);
				break;
			case 'C':
				opts_set_chain(global->opts, argv0, optarg, global_opts_str);
				break;
			case 'K':
				global_set_leafkey(global, argv0, optarg);
				break;
			case 't':
				global_set_leafcertdir(global, argv0, optarg);
				break;
			case 'A':
				global_set_defaultleafcert(global, argv0, optarg);
				break;
			case 'q':
				opts_set_leafcrlurl(global->opts, optarg, global_opts_str);
				break;
			case 'O':
				opts_set_deny_ocsp(global->opts);
				break;
			case 'P':
				opts_set_passthrough(global->opts);
				break;
			case 'a':
				opts_set_clientcrt(global->opts, argv0, optarg, global_opts_str);
				break;
			case 'b':
				opts_set_clientkey(global->opts, argv0, optarg, global_opts_str);
				break;
#ifndef OPENSSL_NO_DH
			case 'g':
				opts_set_dh(global->opts, argv0, optarg, global_opts_str);
				break;
#endif /* !OPENSSL_NO_DH */
#ifndef OPENSSL_NO_ECDH
			case 'G':
				opts_set_ecdhcurve(global->opts, argv0, optarg);
				break;
#endif /* !OPENSSL_NO_ECDH */
#ifdef SSL_OP_NO_COMPRESSION
			case 'Z':
				opts_unset_sslcomp(global->opts);
				break;
#endif /* SSL_OP_NO_COMPRESSION */
			case 's':
				opts_set_ciphers(global->opts, argv0, optarg);
				break;
			case 'U':
				opts_set_ciphersuites(global->opts, argv0, optarg);
				break;
			case 'r':
				opts_force_proto(global->opts, argv0, optarg);
				break;
			case 'R':
				opts_disable_proto(global->opts, argv0, optarg);
				break;
#ifndef OPENSSL_NO_ENGINE
			case 'x':
				global_set_openssl_engine(global, argv0, optarg);
				break;
#endif /* !OPENSSL_NO_ENGINE */
			case 'e':
				if (natengine)
					free(natengine);
				natengine = strdup(optarg);
				if (!natengine)
					oom_die(argv0);
				break;
			case 'E':
				nat_list_engines();
				exit(EXIT_SUCCESS);
				break;
			case 'u':
				global_set_user(global, argv0, optarg);
				break;
			case 'm':
				global_set_group(global, argv0, optarg);
				break;
			case 'p':
				global_set_pidfile(global, argv0, optarg);
				break;
			case 'j':
				global_set_jaildir(global, argv0, optarg);
				break;
			case 'l':
				global_set_connectlog(global, argv0, optarg);
				break;
			case 'J':
				global_set_statslog(global);
				break;
			case 'L':
				global_set_contentlog(global, argv0, optarg);
				break;
			case 'S':
				global_set_contentlogdir(global, argv0, optarg);
				break;
			case 'F':
				global_set_contentlogpathspec(global, argv0, optarg);
				break;
			case 'X':
				global_set_pcaplog(global, argv0, optarg);
				break;
			case 'Y':
				global_set_pcaplogdir(global, argv0, optarg);
				break;
			case 'y':
				global_set_pcaplogpathspec(global, argv0, optarg);
				break;
#ifndef WITHOUT_MIRROR
			case 'I':
				global_set_mirrorif(global, argv0, optarg);
				break;
			case 'T':
				global_set_mirrortarget(global, argv0, optarg);
				break;
#endif /* !WITHOUT_MIRROR */
			case 'W':
				global_set_certgendir_writeall(global, argv0, optarg);
				break;
			case 'w':
				global_set_certgendir_writegencerts(global, argv0, optarg);
				break;
#ifdef HAVE_LOCAL_PROCINFO
			case 'i':
				global_set_lprocinfo(global);
				break;
#endif /* HAVE_LOCAL_PROCINFO */
			case 'M':
				global_set_masterkeylog(global, argv0, optarg);
				break;
			case 'd':
				global_set_daemon(global);
				break;
			case 'D':
				global_set_debug(global);
				if (optarg) {
					global_set_debug_level(optarg);
				}
				break;
			case 'V':
				main_version();
				exit(EXIT_SUCCESS);
			case 'h':
				main_usage();
				exit(EXIT_SUCCESS);
			case '?':
				exit(EXIT_FAILURE);
			default:
				main_usage();
				exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	proxyspec_parse(&argc, &argv, natengine, global, argv0, global_opts_str);

	// We don't need the tmp strs used to clone global opts into proxyspecs anymore
	global_opts_str_free(global_opts_str);
	global_opts_str = NULL;

	/* usage checks before defaults */
	if (global->detach && OPTS_DEBUG(global)) {
		fprintf(stderr, "%s: -d and -D are mutually exclusive.\n",
		                argv0);
		exit(EXIT_FAILURE);
	}
#ifndef WITHOUT_MIRROR
	if (global->mirrortarget && !global->mirrorif) {
		fprintf(stderr, "%s: -T depends on -I.\n", argv0);
		exit(EXIT_FAILURE);
	}
#endif /* !WITHOUT_MIRROR */
	if (!global->spec) {
		fprintf(stderr, "%s: no proxyspec specified.\n", argv0);
		exit(EXIT_FAILURE);
	}
	for (proxyspec_t *spec = global->spec; spec; spec = spec->next) {
		if (spec->connect_addrlen || spec->sni_port)
			continue;
		if (!spec->natengine) {
			fprintf(stderr, "%s: no supported NAT engines "
			                "on this platform.\n"
			                "Only static addr and SNI proxyspecs "
			                "supported.\n", argv0);
			exit(EXIT_FAILURE);
		}
		if (spec->listen_addr.ss_family == AF_INET6 &&
		    !nat_ipv6ready(spec->natengine)) {
			fprintf(stderr, "%s: IPv6 not supported by '%s'\n",
			                argv0, spec->natengine);
			exit(EXIT_FAILURE);
		}
		spec->natlookup = nat_getlookupcb(spec->natengine);
		spec->natsocket = nat_getsocketcb(spec->natengine);
	}
	if (global_has_ssl_spec(global)) {
		if (ssl_init() == -1) {
			fprintf(stderr, "%s: failed to initialize OpenSSL.\n",
			                argv0);
			exit(EXIT_FAILURE);
		}
#ifndef OPENSSL_NO_ENGINE
		if (global->openssl_engine &&
		    ssl_engine(global->openssl_engine) == -1) {
			fprintf(stderr, "%s: failed to enable OpenSSL engine"
			                " %s.\n", argv0, global->openssl_engine);
			exit(EXIT_FAILURE);
		}
#endif /* !OPENSSL_NO_ENGINE */
		main_check_opts(global->opts, argv0);
		for (proxyspec_t *spec = global->spec; spec; spec = spec->next) {
			if (spec->ssl || spec->upgrade)
				main_check_opts(spec->opts, argv0);
		}
	}
#ifdef __APPLE__
	if (global->dropuser && !!strcmp(global->dropuser, "root") &&
	    nat_used("pf")) {
		fprintf(stderr, "%s: cannot use 'pf' proxyspec with -u due "
		                "to Apple bug\n", argv0);
		exit(EXIT_FAILURE);
	}
#endif /* __APPLE__ */

	/* prevent multiple instances running */
	if (global->pidfile) {
		pidfd = sys_pidf_open(global->pidfile);
		if (pidfd == -1) {
			fprintf(stderr, "%s: cannot open PID file '%s' "
			                "- process already running?\n",
			                argv0, global->pidfile);
			exit(EXIT_FAILURE);
		}
	}

#ifndef WITHOUT_USERAUTH
	if (global->opts->user_auth || global_has_userauth_spec(global)) {
		if (!global->userdb_path) {
			fprintf(stderr, "User auth requires a userdb path\n");
			exit(EXIT_FAILURE);
		}
		// @todo Check if we can really pass the db var into the child process for privsep
		// https://www.sqlite.org/faq.html:
		// "Under Unix, you should not carry an open SQLite database across a fork() system call into the child process."
		if (sqlite3_open(global->userdb_path, &global->userdb)) {
			fprintf(stderr, "Error opening user db file: %s\n", sqlite3_errmsg(global->userdb));
			sqlite3_close(global->userdb);
			exit(EXIT_FAILURE);
		}
		if (sqlite3_prepare_v2(global->userdb, "UPDATE users SET atime = ?1 WHERE ip = ?2 AND user = ?3 AND ether = ?4", 200, &global->update_user_atime, NULL)) {
			fprintf(stderr, "Error preparing update_user_atime sql stmt: %s\n", sqlite3_errmsg(global->userdb));
			sqlite3_close(global->userdb);
			exit(EXIT_FAILURE);
		}
	}
#endif /* !WITHOUT_USERAUTH */

	/* dynamic defaults */
	if (!global->opts->ciphers) {
		global->opts->ciphers = strdup(DFLT_CIPHERS);
		if (!global->opts->ciphers)
			oom_die(argv0);
	}
	if (!global->opts->ciphersuites) {
		global->opts->ciphersuites = strdup(DFLT_CIPHERSUITES);
		if (!global->opts->ciphersuites)
			oom_die(argv0);
	}
	for (proxyspec_t *spec = global->spec; spec; spec = spec->next) {
		if (!spec->opts->ciphers) {
			spec->opts->ciphers = strdup(DFLT_CIPHERS);
			if (!spec->opts->ciphers)
				oom_die(argv0);
		}
		if (!spec->opts->ciphersuites) {
			spec->opts->ciphersuites = strdup(DFLT_CIPHERSUITES);
			if (!spec->opts->ciphersuites)
				oom_die(argv0);
		}
	}
	if (!global->dropuser && !geteuid() && !getuid() &&
	    sys_isuser(DFLT_DROPUSER)) {
#ifdef __APPLE__
		/* Apple broke ioctl(/dev/pf) for EUID != 0 so we do not
		 * want to automatically drop privileges to nobody there
		 * if pf has been used in any proxyspec */
		if (!nat_used("pf")) {
#endif /* __APPLE__ */
		global->dropuser = strdup(DFLT_DROPUSER);
		if (!global->dropuser)
			oom_die(argv0);
#ifdef __APPLE__
		}
#endif /* __APPLE__ */
	}
	if (global->dropuser && sys_isgeteuid(global->dropuser)) {
		if (global->dropgroup) {
			fprintf(stderr, "%s: cannot use -m when -u is "
			        "current user\n", argv0);
			exit(EXIT_FAILURE);
		}
		free(global->dropuser);
		global->dropuser = NULL;
	}

	/* usage checks after defaults */
	if (global->dropgroup && !global->dropuser) {
		fprintf(stderr, "%s: -m depends on -u\n", argv0);
		exit(EXIT_FAILURE);
	}

	/* Warn about options that require per-connection privileged operations
	 * to be executed through privsep, but only if dropuser is set and is
	 * not root, because privsep will fastpath in that situation, skipping
	 * the latency-incurring overhead. */
	int privsep_warn = 0;
	if (global->dropuser) {
		if (global->contentlog_isdir) {
			log_dbg_printf("| Warning: -F requires a privileged "
			               "operation for each connection!\n");
			privsep_warn = 1;
		}
		if (global->contentlog_isspec) {
			log_dbg_printf("| Warning: -S requires a privileged "
			               "operation for each connection!\n");
			privsep_warn = 1;
		}
		if (global->pcaplog_isdir) {
			log_dbg_printf("| Warning: -Y requires a privileged "
			               "operation for each connection!\n");
			privsep_warn = 1;
		}
		if (global->pcaplog_isspec) {
			log_dbg_printf("| Warning: -y requires a privileged "
			               "operation for each connection!\n");
			privsep_warn = 1;
		}
		if (global->certgendir) {
			log_dbg_printf("| Warning: -w/-W require a privileged "
			               "op for each connection!\n");
			privsep_warn = 1;
		}
	}
	if (privsep_warn) {
		log_dbg_printf("| Privileged operations require communication "
		               "between parent and child process\n"
		               "| and will negatively impact latency and "
		               "performance on each connection.\n");
	}

	/* debug log, part 1 */
	if (OPTS_DEBUG(global)) {
		main_version();
	}

	/* generate leaf key */
	if (global_has_ssl_spec(global) && global_has_cakey_spec(global) && !global->leafkey) {
		global->leafkey = ssl_key_genrsa(global->leafkey_rsabits);
		if (!global->leafkey) {
			fprintf(stderr, "%s: error generating RSA key:\n",
			                argv0);
			ERR_print_errors_fp(stderr);
			exit(EXIT_FAILURE);
		}
		if (OPTS_DEBUG(global)) {
			log_dbg_printf("Generated %i bit RSA key for leaf "
			               "certs.\n", global->leafkey_rsabits);
		}
	}
	if (global->leafkey && global->certgendir) {
		char *keyid, *keyfn;
		int prv;
		FILE *keyf;

		keyid = ssl_key_identifier(global->leafkey, 0);
		if (!keyid) {
			fprintf(stderr, "%s: error generating key id\n", argv0);
			exit(EXIT_FAILURE);
		}

		prv = asprintf(&keyfn, "%s/%s.key", global->certgendir, keyid);
		if (prv == -1) {
			fprintf(stderr, "%s: %s (%i)\n", argv0,
			                strerror(errno), errno);
			exit(EXIT_FAILURE);
		}

		if (!(keyf = fopen(keyfn, "w"))) {
			fprintf(stderr, "%s: Failed to open '%s' for writing: "
			                "%s (%i)\n", argv0, keyfn,
			                strerror(errno), errno);
			exit(EXIT_FAILURE);
		}
		if (!PEM_write_PrivateKey(keyf, global->leafkey, NULL, 0, 0,
		                                           NULL, NULL)) {
			fprintf(stderr, "%s: Failed to write key to '%s': "
			                "%s (%i)\n", argv0, keyfn,
			                strerror(errno), errno);
			exit(EXIT_FAILURE);
		}
		fclose(keyf);
	}

	/* debug log, part 2 */
	if (OPTS_DEBUG(global)) {
		char *s = opts_proto_dbg_dump(global->opts);
		if (!s) {
			fprintf(stderr, "%s: out of memory\n", argv0);
			exit(EXIT_FAILURE);
		}
		log_dbg_printf("Global %s\n", s);
		free(s);

		log_dbg_printf("proxyspecs:\n");
		for (proxyspec_t *spec = global->spec; spec; spec = spec->next) {
			char *specstr = proxyspec_str(spec);
			if (!specstr) {
				fprintf(stderr, "%s: out of memory\n", argv0);
				exit(EXIT_FAILURE);
			}
			log_dbg_printf("- %s\n", specstr);
			free(specstr);
		}
#ifndef OPENSSL_NO_ENGINE
		if (global->openssl_engine) {
			log_dbg_printf("Loaded OpenSSL engine %s\n",
			               global->openssl_engine);
		}
#endif /* !OPENSSL_NO_ENGINE */
		if (global->opts->cacrt) {
			char *subj = ssl_x509_subject(global->opts->cacrt);
			log_dbg_printf("Loaded CA: '%s'\n", subj);
			free(subj);
#ifdef DEBUG_CERTIFICATE
			log_dbg_print_free(ssl_x509_to_str(global->opts->cacrt));
			log_dbg_print_free(ssl_x509_to_pem(global->opts->cacrt));
#endif /* DEBUG_CERTIFICATE */
		} else {
			log_dbg_printf("No CA loaded.\n");
		}
		for (proxyspec_t *spec = global->spec; spec; spec = spec->next) {
			if (spec->opts->cacrt) {
				char *subj = ssl_x509_subject(spec->opts->cacrt);
				log_dbg_printf("Loaded ProxySpec CA: '%s'\n", subj);
				free(subj);
#ifdef DEBUG_CERTIFICATE
				log_dbg_print_free(ssl_x509_to_str(spec->opts->cacrt));
				log_dbg_print_free(ssl_x509_to_pem(spec->opts->cacrt));
#endif /* DEBUG_CERTIFICATE */
			} else {
				log_dbg_printf("No ProxySpec CA loaded.\n");
			}
		}
		log_dbg_printf("SSL/TLS leaf certificates taken from:\n");
		if (global->leafcertdir) {
			log_dbg_printf("- Matching PEM file in %s\n",
			               global->leafcertdir);
		}
		if (global->defaultleafcert) {
			log_dbg_printf("- Default leaf key\n");
		// @todo Debug print the cakey and passthrough opts for proxspecs too?
		} else if (global->opts->cakey) {
			log_dbg_printf("- Global generated on the fly\n");
		} else if (global->opts->passthrough) {
			log_dbg_printf("- Global passthrough without decryption\n");
		} else {
			log_dbg_printf("- Global connection drop\n");
		}
	}

	/*
	 * Initialize as much as possible before daemon() in order to be
	 * able to provide direct feedback to the user when failing.
	 */
	if (cachemgr_preinit() == -1) {
		fprintf(stderr, "%s: failed to preinit cachemgr.\n", argv0);
		exit(EXIT_FAILURE);
	}
	if (log_preinit(global) == -1) {
		fprintf(stderr, "%s: failed to preinit logging.\n", argv0);
		exit(EXIT_FAILURE);
	}
	if (nat_preinit() == -1) {
		fprintf(stderr, "%s: failed to preinit NAT lookup.\n", argv0);
		exit(EXIT_FAILURE);
	}

	/* Load certs before dropping privs but after cachemgr_preinit() */
	if (global->leafcertdir) {
		if (sys_dir_eachfile(global->leafcertdir,
		                     main_load_leafcert, global) == -1) {
			fprintf(stderr, "%s: failed to load certs from %s\n",
			                argv0, global->leafcertdir);
			exit(EXIT_FAILURE);
		}
	}

	/* Detach from tty; from this point on, only canonicalized absolute
	 * paths should be used (-j, -F, -S). */
	if (global->detach) {
		if (OPTS_DEBUG(global)) {
			log_dbg_printf("Detaching from TTY, see syslog for "
			               "errors after this point\n");
		}
		if (daemon(0, 0) == -1) {
			fprintf(stderr, "%s: failed to detach from TTY: %s\n",
			                argv0, strerror(errno));
			exit(EXIT_FAILURE);
		}
		log_err_mode(LOG_ERR_MODE_SYSLOG);
	}

	if (global->pidfile && (sys_pidf_write(pidfd) == -1)) {
		log_err_level_printf(LOG_CRIT, "Failed to write PID to PID file '%s': %s (%i)"
		               "\n", global->pidfile, strerror(errno), errno);
		return -1;
	}

	descriptor_table_size = getdtablesize();

	/* Fork into parent monitor process and (potentially unprivileged)
	 * child process doing the actual work.  We request 6 privsep client
	 * sockets: five logger threads, and the child process main thread,
	 * which will become the main proxy thread.  First slot is main thread,
	 * remaining slots are passed down to log subsystem. */
	int clisock[6];
	if (privsep_fork(global, clisock,
	                 sizeof(clisock)/sizeof(clisock[0]), &rv) != 0) {
		/* parent has exited the monitor loop after waiting for child,
		 * or an error occurred */
		if (global->pidfile) {
			sys_pidf_close(pidfd, global->pidfile);
		}
		goto out_parent;
	}
	/* child */

	/* close pidfile in child */
	if (global->pidfile)
		close(pidfd);

	/* Initialize proxy before dropping privs */
	proxy_ctx_t *proxy = proxy_new(global, clisock[0]);
	if (!proxy) {
		log_err_level_printf(LOG_CRIT, "Failed to initialize proxy.\n");
		exit(EXIT_FAILURE);
	}

	/* Drop privs, chroot */
	if (sys_privdrop(global->dropuser, global->dropgroup,
	                 global->jaildir) == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to drop privileges: %s (%i)\n",
		               strerror(errno), errno);
		exit(EXIT_FAILURE);
	}
	log_dbg_printf("Dropped privs to user %s group %s chroot %s\n",
	               global->dropuser  ? global->dropuser  : "-",
	               global->dropgroup ? global->dropgroup : "-",
	               global->jaildir   ? global->jaildir   : "-");
	if (ssl_reinit() == -1) {
		fprintf(stderr, "%s: failed to reinit SSL\n", argv0);
		goto out_sslreinit_failed;
	}

	/* Post-privdrop/chroot/detach initialization, thread spawning */
	if (log_init(global, proxy, &clisock[1]) == -1) {
		fprintf(stderr, "%s: failed to init log facility: %s\n",
		                argv0, strerror(errno));
		goto out_log_failed;
	}
	if (cachemgr_init() == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to init cache manager.\n");
		goto out_cachemgr_failed;
	}
	if (nat_init() == -1) {
		log_err_level_printf(LOG_CRIT, "Failed to init NAT state table lookup.\n");
		goto out_nat_failed;
	}

	int proxy_rv = proxy_run(proxy);
	if (proxy_rv == 0) {
		rv = EXIT_SUCCESS;
	} else if (proxy_rv > 0) {
		/*
		 * We terminated because of receiving a signal.  For our normal
		 * termination signals as documented in the man page, we want
		 * to return with EXIT_SUCCESS.  For other signals, which
		 * should be considered abnormal terminations, we want to
		 * return an exit status of 128 + signal number.
		 */
		if (proxy_rv == SIGTERM || proxy_rv == SIGINT) {
			rv = EXIT_SUCCESS;
		} else {
			rv = 128 + proxy_rv;
		}
	}

	log_finest_main_va("EXIT closing privsep clisock=%d", clisock[0]);
	privsep_client_close(clisock[0]);

	proxy_free(proxy);
	nat_fini();
out_nat_failed:
	cachemgr_fini();
out_cachemgr_failed:
	log_fini();
out_sslreinit_failed:
out_log_failed:
out_parent:
	global_free(global);
	ssl_fini();
	if (natengine)
		free(natengine);
	return rv;
}

/* vim: set noet ft=c: */

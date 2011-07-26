#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "output.h"
#include "wgetlite.h"
#include "term.h"
#include "cookies.h"
#include "connections.h"

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

/* #define USER_AGENT "wgetlite/0.9 (linux)"; */
#define USER_AGENT NULL

struct cfg global_cfg;
const char *argv0;


void sigh(int sig)
{
	if(sig != SIGPIPE)
		fprintf(stderr, "wgetlite: caught signal %d\n", sig);
	exit(127 + sig);
}

void cleanup(void)
{
	if(global_cfg.logf)
		fclose(global_cfg.logf);
	cookies_end();
}

void verbosity_change(int dir)
{
#define v global_cfg.verbosity
	if(dir < 0){
		while(v > 0       && dir ++< 0)
			v--;
	}else{
		while(v < OUT_ERR && dir --> 0)
			v++;
	}
#undef v
}

void proxy()
{
	char *p;

	p = getenv("HTTP_PROXY");
	if(!p){
		p = getenv("http_proxy");
		if(!p)
			p = "";
	}

	global_cfg.http_proxy = p;
	p = strchr(p, ':');
	if(p){
		*p++ = '\0';
		global_cfg.http_proxy_port = p;
	}else{
		global_cfg.http_proxy_port = "8080";
	}

	if(*global_cfg.http_proxy)
		output_err(OUT_VERBOSE, "Using HTTP Proxy: %s:%s",
			global_cfg.http_proxy,
			global_cfg.http_proxy_port);
}

int main(int argc, char **argv)
{
	int ret = 0, ch;
	const char *log_fname = NULL;
	const char *cookie_fname = NULL;

	argv0 = *argv;

	memset(&global_cfg, 0, sizeof global_cfg);
	global_cfg.verbosity = OUT_INFO;

	setbuf(stdout, NULL);

	signal(SIGPIPE,  sigh);
	signal(SIGWINCH, term_winch);

	global_cfg.prog_dot = !isatty(STDERR_FILENO);
	global_cfg.user_agent = USER_AGENT;

	while((ch = getopt(argc, argv, "vqdcfU:C:o:O:")) != -1)
		switch(ch){
			case 'v':
			case 'q':
				verbosity_change(ch == 'v' ? -1 : +1);
				break;

			case 'f': global_cfg.overwrite = 1;           break;
			case 'c': global_cfg.partial = 1;             break;
			case 'd': global_cfg.prog_dot = 1;            break;
			case 'U': global_cfg.user_agent = optarg;     break;
			case 'C': cookie_fname = optarg;              break;
			case 'o': log_fname = optarg;                 break;
			case 'O': global_cfg.out_fname = optarg;      break;

			default:
usage:
				fprintf(stderr,
						"Usage: %s [OPTS] url\n"
						" -v: Increase Verboseness\n"
						" -q: Decrease Verboseness\n"
						" -d: Dot-Style Progress\n"
						" -c: Attempt to resume\n"
						" -U: Specify User-Agent\n"
						" -C: Specify Cookie file\n"
						" -o: Log to file\n"
						" -O: Save to file\n"
						" -f: Overwrite files\n"
						, *argv);
				return 1;
		}

	if(optind == argc){
		fprintf(stderr, "need url(s)\n");
		goto usage;
	}

	if(log_fname){
		if(!strcmp(log_fname, "-")){
			global_cfg.logf = stdout;
		}else if(!(global_cfg.logf = fopen(log_fname, "a"))){
			output_err(OUT_ERR, "open log: %s: %s", log_fname, strerror(errno));
			return 1;
		}
	}

	atexit(cleanup);

	if(cookie_fname)
		cookies_load(cookie_fname);

	proxy();

	for(; optind < argc; optind++)
		ret |= wget(argv[optind], 0);

	connection_fin();

	return ret;
}

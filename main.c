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

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

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

int main(int argc, char **argv)
{
	int i, proc_opts = 1;
	char *url = NULL;
	const char *log_fname = NULL;
	const char *cookie_fname = NULL;
	const struct
	{
		char opt;
		const char **ptr;
		int can_empty;
	} opts[] = {
		{ 'o', &log_fname, 0 },
		{ 'O', &global_cfg.out_fname, 0 },
		{ 'U', &global_cfg.user_agent, 1 },
		{ 'C', &cookie_fname, 0 }
	};

	argv0 = *argv;

	memset(&global_cfg, 0, sizeof global_cfg);
	global_cfg.verbosity = OUT_INFO;

	setbuf(stdout, NULL);

	signal(SIGPIPE,  sigh);
	signal(SIGWINCH, term_winch);

	global_cfg.prog_dot = !isatty(STDERR_FILENO);
	global_cfg.user_agent = "wgetlite/0.9 (linux)";

	for(i = 1; i < argc; i++)
		if(proc_opts && *argv[i] == '-')
			switch(argv[i][1]){
				case 'c':
				case 'd':
				case 'v':
				case 'q':
				{
					char *iter;

					for(iter = argv[i] + 1; *iter; iter++)
						switch(*iter){
							case 'c': global_cfg.partial = 1;  break;
							case 'd': global_cfg.prog_dot = 1; break;

							case 'v':
							case 'q':
								verbosity_change(*iter == 'v' ? -1 : +1);
								break;

							default:
								goto usage;
						}
					break;
				}

				case '-':
				if(!argv[i][2]){
					proc_opts = 0;
					break;
				}
				/* fall through */

				default:
				{
					unsigned int j;

					for(j = 0; j < ARRAY_LEN(opts); j++)
						if(argv[i][1] == opts[j].opt)
							break;

					if(j < ARRAY_LEN(opts)){
						if(argv[i][2] == '\0'){
							if(!(*opts[j].ptr = argv[++i]) || *argv[i] == '\0'){
								if(opts[j].can_empty)
									*opts[j].ptr = NULL;
								else{
									fprintf(stderr, "option \"%s\" can't be empty\n",
											argv[i-1]);
									goto usage;
								}
							}
						}else{
							*opts[j].ptr = argv[i] + 2;
							if(**opts[j].ptr == '\0'){
								if(opts[j].can_empty)
									*opts[j].ptr = NULL;
								else{
									fprintf(stderr, "option \"%s\" can't be empty\n",
											argv[i]);
									goto usage;
								}
							}
						}
					}else
						goto usage_print;

					break;
				}
			}
		else if(!url){
			url = argv[i];
			proc_opts = 0;
		}else{
		usage_print:
			fprintf(stderr, "Unrecognised option \"%s\"\n", argv[i]);
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
					, *argv);
			return 1;
		}

	if(log_fname){
		if(!strcmp(log_fname, "-")){
			global_cfg.logf = stdout;
		}else if(!(global_cfg.logf = fopen(log_fname, "a"))){
			output_err(OUT_ERR, "open log: %s: %s", log_fname, strerror(errno));
			return 1;
		}
	}

	if(!url){
		fprintf(stderr, "need url\n");
		goto usage;
	}

	atexit(cleanup);

	if(cookie_fname)
		cookies_load(cookie_fname);

	return wget(url, 0);
}

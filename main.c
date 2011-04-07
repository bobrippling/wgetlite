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

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

struct cfg global_cfg;
const char *argv0;


void sigh(int sig)
{
	fputs("we get signal!\n", stderr);
	exit(sig);
}

void cleanup(void)
{
	if(global_cfg.logf)
		fclose(global_cfg.logf);
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
	int i;
	char *url = NULL;
	const char *log_fname = NULL;
	const struct
	{
		char opt;
		const char **ptr;
		int can_empty;
	} opts[] = {
		{ 'o', &log_fname, 0 },
		{ 'O', &global_cfg.out_fname, 0 },
		{ 'U', &global_cfg.user_agent, 1 }
	};

	argv0 = *argv;

	memset(&global_cfg, 0, sizeof global_cfg);
	global_cfg.verbosity = OUT_INFO;

	setbuf(stdout, NULL);

	signal(SIGPIPE,  sigh);
	signal(SIGWINCH, term_winch);

	global_cfg.prog_dot = !isatty(1);
	global_cfg.user_agent = "wgetlite/0.9 (linux)";

	for(i = 1; i < argc; i++)
		if(*argv[i] == '-')
			switch(argv[i][1]){
				case 'c':
					global_cfg.partial = 1;
					break;

				case 'd':
					global_cfg.prog_dot = 1;
					break;

				case 'v':
				case 'q':
				{
					int j = 1;
					char *s;

					for(s = argv[i] + 2; *s == argv[i][1]; s++, j++);
					if(*s != '\0')
						goto usage;

					verbosity_change((argv[i][1] == 'v' ? -1 : +1) * j);
					break;
				}

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
								else
									goto usage;
							}
						}else{
							*opts[j].ptr = argv[i] + 2;
							if(**opts[j].ptr == '\0'){
								if(opts[j].can_empty)
									*opts[j].ptr = NULL;
								else
									goto usage;
							}
						}
					}else
						goto usage;

					break;
				}
			}
		else if(!url){
			url = argv[i];
		}else{
		usage:
			fprintf(stderr, "Usage: %s [-v] [-q] [-d] [-c] [-o log] [-O file] url\n", *argv);
			return 1;
		}

	if(log_fname){
		if(!strcmp(log_fname, "-")){
			global_cfg.logf = stdout;
		}else if(!(global_cfg.logf = fopen(log_fname, "a"))){
			output_err(OUT_ERR, "open log: %s: %s\n", log_fname, strerror(errno));
			return 1;
		}
	}

	if(!url)
		goto usage;

	atexit(cleanup);

	return wget(url, 0);
}

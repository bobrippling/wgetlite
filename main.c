#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "output.h"
#include "wgetlite.h"
#include "term.h"

struct cfg global_cfg;
const char *argv0;


void sigh(int sig)
{
	fputs("we get signal!\n", stderr);
	exit(sig);
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

	argv0 = *argv;

	memset(&global_cfg, 0, sizeof global_cfg);
	global_cfg.verbosity = OUT_INFO;

	setbuf(stdout, NULL);

	signal(SIGPIPE,  sigh);
	signal(SIGWINCH, term_winch);

#define ARG(x) !strcmp(argv[i], "-" x)

	global_cfg.prog_dot = !isatty(1);

	for(i = 1; i < argc; i++)
		if(     ARG("c")) global_cfg.partial = 1;
		else if(ARG("d")) global_cfg.prog_dot = 1;

		else if(argv[i][0] == '-' && (argv[i][1] == 'v' || argv[i][1] == 'q')){
			int j = 1;
			char *s;

			for(s = argv[i] + 2; *s == argv[i][1]; s++, j++);
			if(*s != '\0')
				goto usage;

			verbosity_change((argv[i][1] == 'v' ? -1 : +1) * j);

		}else if(!strncmp(argv[i], "-O", 2)){
			if(argv[i][2] == '\0'){
				if(!(global_cfg.out_fname = argv[++i]))
					goto usage;
			}else
				global_cfg.out_fname = argv[i] + 2;

		}else if(!url){
			url = argv[i];

		}else{
		usage:
			fprintf(stderr, "Usage: %s [-v] [-q] [-d] [-c] [-O file] url\n", *argv);
			return 1;
		}

	if(!url)
		goto usage;

	return wget(url);
}

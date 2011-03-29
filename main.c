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
	if(dir < 0 && v > 0)
		v--;
	else if(v < OUT_ERR)
		v++;
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
#define verbosity_inc() verbosity_change(-1)
#define verbosity_dec() verbosity_change(+1)

	global_cfg.prog_dot = !isatty(1);

	for(i = 1; i < argc; i++)
		if(     ARG("c")) global_cfg.partial = 1;
		else if(ARG("q")) verbosity_dec();
		else if(ARG("v")) verbosity_inc();
		else if(ARG("d")) global_cfg.prog_dot = 1;

		else if(ARG("O")){
			if(!(global_cfg.out_fname = argv[++i]))
				goto usage;

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

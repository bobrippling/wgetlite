#include <stdio.h>
#include "progress.h"
#include "output.h"
#include "wgetlite.h"
#include "term.h"

#define CLR_TO_EOL "\x1b[K"
#define PROGRESS_CAN_SHOW() (global_cfg.verbosity <= OUT_WARN)
#define PROGRESS_TERMINAL() (!global_cfg.prog_dot)

extern struct cfg global_cfg;


void progress_unknown()
{
	static int i = 0;
	if(PROGRESS_CAN_SHOW())
		fprintf(stderr, "\r%c...", "/-\\|"[i++ % 4]);
}

void progress_fin(int this, int total)
{
	if(PROGRESS_CAN_SHOW()){
		if(total)
			progress_show(this, total);
		else if(PROGRESS_TERMINAL())
			progress_unknown();

		if(PROGRESS_TERMINAL())
			fputc('\n', stderr);
	}
}

void progress_incomplete()
{
#define EOS "Unexpected end-of-stream"

	if(PROGRESS_CAN_SHOW()){
		if(PROGRESS_TERMINAL())
			fputs("\r" CLR_TO_EOL EOS "\n", stderr);
		else
			fputs(EOS "\n", stderr);
	}

#undef EOS
}

void progress_show(int this, int total)
{
	if(PROGRESS_CAN_SHOW()){
		register int percent = 100.0f * (float)this / (float)total;
		char *pre, *post;
		char *sizstr;
		int div;

		if(total > 1024){
			div = 1024;
			sizstr = "KB";
		}else{
			div = 1;
			sizstr = "B";
		}

		if(PROGRESS_TERMINAL()){
			pre = CLR_TO_EOL;
			post = "\r";
		}else{
			pre = "";
			post = "\n";
		}

		fprintf(stderr,
				"%s%d%% (%d %s / %d %s)%s",
				pre, percent,
				this  / div, sizstr,
				total / div, sizstr,
				post);
	}
}

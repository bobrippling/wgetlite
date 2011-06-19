#include <stdio.h>
#include "progress.h"
#include "output.h"
#include "wgetlite.h"
#include "term.h"

#define CLR_TO_EOL "\x1b[K"
#define PROGRESS_CAN_SHOW() (global_cfg.verbosity <= OUT_WARN)
#define PROGRESS_TERMINAL() (!global_cfg.prog_dot)

extern struct cfg global_cfg;


static void progress_get_div(long total, int *div, const char **sizstr)
{
	if(total > 1024 * 1024){
		*div    = 1024 * 1024;
		*sizstr = "M";
	}else if(total > 1024){
		*div    = 1024;
		*sizstr = "K";
	}else{
		*div    = 1;
		*sizstr = "";
	}
}

void progress_unknown(long sofar, long bps)
{
	static int i = 0;

	if(PROGRESS_CAN_SHOW()){
		int div;
		const char *spd;

		progress_get_div(bps, &div, &spd);

		fprintf(stderr, "\r%ldB %ld %sB/s %c...",
				sofar,
				bps / div, spd,
				"/-\\|"[i++ % 4]);
	}
}

void progress_fin(long this, long total)
{
	if(PROGRESS_CAN_SHOW()){
		if(total)
			progress_show(this, total, 0);
		else if(PROGRESS_TERMINAL())
			progress_unknown(this, 0);

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

void progress_show(long this, long total, long bps)
{
	if(PROGRESS_CAN_SHOW()){
		register int percent = 100.0f * (float)this / (float)total;
		const char *pre, *post;
		const char *sizstr, *bpssizstr;
		int div, bpsdiv;

		if(PROGRESS_TERMINAL()){
			pre = CLR_TO_EOL;
			post = "\r";
		}else{
			pre = "";
			post = "\n";
		}

		progress_get_div(total, &div,    &sizstr);
		progress_get_div(bps,   &bpsdiv, &bpssizstr);

		fprintf(stderr,
				"%s%d%% (%ld %sB / %ld %sB) %ld%sB/s %s",
				pre, percent,
				this  / div, sizstr,
				total / div, sizstr,
				bps / bpsdiv, bpssizstr,
				post);
	}
}

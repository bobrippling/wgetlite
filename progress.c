#include <stdio.h>
#include "progress.h"
#include "output.h"
#include "wgetlite.h"
#include "term.h"
#include "util.h"

#define CLR_TO_EOL "\x1b[K"
#define PROGRESS_CAN_SHOW() (global_cfg.verbosity <= OUT_WARN)
#define PROGRESS_TERMINAL() (!global_cfg.prog_dot)

extern struct cfg global_cfg;


void progress_unknown(long sofar, long bps)
{
	if(PROGRESS_CAN_SHOW()){
		static int i = 0;

		fprintf(stderr, "\r%s %s %c...",
				bytes_to_str(sofar),
				bytes_to_str(bps),
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

		if(PROGRESS_TERMINAL()){
			pre = CLR_TO_EOL;
			post = "\r";
		}else{
			pre = "";
			post = "\n";
		}

		fprintf(stderr,
				"%s%d%% (%s / %s) %s/s %s",
				pre,
				percent,
				bytes_to_str(this),
				bytes_to_str(total),
				bytes_to_str(bps),
				post);
	}
}

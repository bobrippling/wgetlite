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
		char buf[2][64];

		fprintf(stderr, "\r%s %s %c...",
				bytes_to_str(buf[0], sizeof buf[0], sofar),
				bytes_to_str(buf[1], sizeof buf[1], bps),
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
		char buf[3][64];

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
				bytes_to_str(buf[0], sizeof buf[0], this),
				bytes_to_str(buf[1], sizeof buf[1], total),
				bytes_to_str(buf[2], sizeof buf[2], bps),
				post);
	}
}

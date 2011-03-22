#include <stdio.h>
#include "progress.h"
#include "output.h"
#include "wgetlite.h"

#define CLR_TO_EOL "\x1b[K"
#define PROGRESS_CAN_SHOW() (global_cfg.verbosity <= OUT_WARN)

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
		if(this)
			progress_show(this, total);
		else
			progress_unknown();
		fputc('\n', stderr);
	}
}

void progress_incomplete()
{
	if(PROGRESS_CAN_SHOW())
		fputs("\r" CLR_TO_EOL "Unexpected end-of-stream\n", stderr);
}

void progress_show(int this, int total)
{
#define PROG(fmt, div)  \
		fprintf(stderr, CLR_TO_EOL "%d%% (%d " fmt " / %d " fmt ")\r", percent, this / div, total / div)

	if(PROGRESS_CAN_SHOW()){
		register int percent = 100.0f * (float)this / (float)total;

		if(this > 1024)
			PROG("KB", 1024);
		else
			PROG("B", 1);
	}
}

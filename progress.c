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

		fputc('\n', stderr);
	}
}

void progress_incomplete()
{
	if(PROGRESS_CAN_SHOW()){
#define EOS "Unexpected end-of-stream"
		if(PROGRESS_TERMINAL())
			fputs("\r" CLR_TO_EOL EOS "\n", stderr);
		else
			fputs("\n" EOS "\n", stderr);
#undef EOS
	}
}

void progress_show(int this, int total)
{
#define PROG(fmt, div)  \
		fprintf(stderr, CLR_TO_EOL "%d%% (%d " fmt " / %d " fmt ")\r", percent, this / div, total / div)

	if(PROGRESS_CAN_SHOW()){
		register int percent = 100.0f * (float)this / (float)total;

		if(PROGRESS_TERMINAL()){
			if(this > 1024)
				PROG("KB", 1024);
			else
				PROG("B", 1);
		}else{
			static int midblock = 0;
			int x, y;
			term_xy(&x, &y);

			if(!midblock){
				fprintf(stderr, " %04dB", this - this % 50); /* TODO: kb */
				midblock = 1;
			}

			while(this > 0){
				int blockno = this - this % 10;
				int dots = this - 10;

				while(dots-- > 0)
					fputc('.', stderr);

				this -= 10;

				fputc(' ', stderr);
			}
		}
	}
}

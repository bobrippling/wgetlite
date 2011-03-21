#include <stdio.h>
#include "progress.h"

#define CLR_TO_EOL "\x1b[K"

void progress_unknown()
{
	static int i = 0;
	printf("\r%c...", "/-\\|"[i++ % 4]);
}

void progress_fin(int this, int total)
{
	if(this)
		progress_show(this, total);
	else
		progress_unknown();
	putchar('\n');
}

void progress_incomplete()
{
	puts("\r" CLR_TO_EOL "Unexpected end-of-stream");
}

void progress_show(int this, int total)
{
#define PROG(fmt, div)  \
		printf(CLR_TO_EOL "%d%% (%d " fmt " / %d " fmt ")\r", percent, this / div, total / div)

	register int percent = 100.0f * (float)this / (float)total;

	if(this > 1024)
		PROG("KB", 1024);
	else
		PROG("B", 1);
}

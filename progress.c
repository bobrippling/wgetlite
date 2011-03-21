#include <stdio.h>
#include "progress.h"

#define PROGRESS_RESET "\r\x1b[K"

void progress_unknown()
{
	static int i = 0;
	printf("\r%c...", "/-\\|"[i++ % 4]);
}

void progress_fin()
{
	putchar('\n');
}

void progress_incomplete()
{
	puts(PROGRESS_RESET "Unexpected end-of-stream");
}

void progress_show(int this, int total)
{
#define PROG(fmt, div)  \
		printf(PROGRESS_RESET "%d%% (%d " fmt " / %d " fmt ")", percent, this / div, total / div)

	register int percent = 100.0f * (float)this / (float)total;

	if(this > 1024)
		PROG("KB", 1024);
	else
		PROG("B", 1);
}

#include <stdio.h>
#include "progress.h"

void progress_unknown()
{
	static int i = 0;
	printf("\r%c...", "/-\\|"[i++ % 4]);
}

void progress_fin()
{
	putchar('\n');
}

void progress_show(int this, int total)
{
#define PROG(fmt, div)  \
		printf("\r%d%% (%d " fmt " / %d " fmt ")", percent, this / div, total / div)

	register int percent = 100.0f * (float)this / (float)total;

	if(this > 1024)
		PROG("KB", 1024);
	else
		PROG("B", 1);
}

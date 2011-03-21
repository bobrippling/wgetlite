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
	register int percent = 100.0f * (float)this / (float)total;
	printf("\r%d%% (%d KB / %d KB)", percent, this, total);
}

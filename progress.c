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

void progress_show(int this, int total, const char *file)
{
	printf("\r\"%s\": %d / %d", file, this, total);
}

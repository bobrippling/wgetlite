#include <sys/time.h>
#include <stdlib.h>

#include "util.h"

long mstime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

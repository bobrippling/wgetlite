#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "output.h"

int file_copy(const char *src, FILE **pdest)
{
	FILE *in = fopen(src, "r"), *dest = *pdest;
	char buffer[4096];
	size_t nread;

	if(!in){
		output_err(OUT_ERR, "open: %s: %s", src, strerror(errno));
		return 1;
	}

	while((nread = fread(buffer, 1, sizeof buffer, in)))
		if(fwrite(buffer, 1, nread, dest) != nread){
			output_err(OUT_ERR, "write: %s", strerror(errno));
			goto bail;
		}

	if(ferror(in)){
		output_err(OUT_ERR, "read: %s", strerror(errno));
		goto bail;
	}

	fclose(in);
	return 0;
bail:
	fclose(in);
	return 1;
}

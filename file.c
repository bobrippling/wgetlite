#include <stdio.h>
#include <string.h>
#include <errno.h>

int file_copy(const char *src, FILE *dest)
{
	FILE *in = fopen(src, "r");
	char buffer[4096];
	size_t nread;

	if(!in){
		fprintf(stderr, "open: %s: %s\n", src, strerror(errno));
		return 1;
	}

	while((nread = fread(buffer, 1, sizeof buffer, in)))
		if(fwrite(buffer, 1, nread, dest) != nread){
			fprintf(stderr, "write: %s\n", strerror(errno));
			goto bail;
		}

	if(ferror(in)){
		fprintf(stderr, "read: %s\n", strerror(errno));
		goto bail;
	}

	fclose(in);
	return 0;
bail:
	fclose(in);
	return 1;
}

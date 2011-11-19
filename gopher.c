#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>

#include "output.h"
#include "wgetlite.h"
#include "gopher.h"
#include "util.h"

/*
 * This is a simple Gopher (RFC 1436) support.
 */
int gopher_retrieve(struct wgetfile *finfo)
{
	FILE *f;
	char *token;
	char type, trailbuf[4];
	int rv;
	long flength;

	if(strlen(finfo->host_file) > 1) {
		token = &finfo->host_file[2];
		type = finfo->host_file[1];
	} else {
		/*
		 * If there is nothing given, it likely may be
		 * some menu.
		 */
		token = finfo->host_file;
		type = '1';
	}

	fdprintf(finfo->sock, "%s\r\n", token);

	f = wget_open(finfo, NULL);
	if(!f)
		return 1;

	/* finfo, file, len, sofar, closefd */
	rv = generic_transfer(finfo, f, 0, 0, 0);
	if(rv)
		goto closereturn;

	switch(type){
		case '0':
		case '1':
			/*
			 * We should be at the end of the file.
			 */
			flength = ftell(f);
			if(flength < 0) {
				rv = 1;
				goto closereturn;
			}
			if(flength < 4)
				break;
			if(fseek(f, flength - 4, SEEK_SET) < 0) {
				rv = 1;
				goto closereturn;
			}
			memset(trailbuf, 0, sizeof(trailbuf));
			if(!fread(trailbuf, 1, 4, f)) {
				rv = 1;
				goto closereturn;
			}

			/*
			 * Check for maldesigned gopher servers using '\n'
			 * instead of '\r\n'.
			 */
			if(!memcmp(trailbuf+2, ".\n", 2)) {
				flength -= 2;
			} else if(!memcmp(trailbuf+1, ".\r\n", 3)) {
				flength -= 3;
			} else {
				break;
			}
			if(ftruncate(fileno(f), flength) < 0) {
				rv = 1;
				goto closereturn;
			}
			break;
	}

closereturn:
	rv |= wget_close(finfo, f);
	if(!rv)
		wget_success(finfo);

	return rv;
}


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
 * This is a simple Gopher (RFC 1436) support. It may handle
 * item type '0' and '1' aswell, but would require a rewrite
 * generic_transfer(), to preserve the line formatting and
 * detect a '\r\n.\r\n' sequence. For now this is the most simple
 * solution. 
 */
int gopher_retrieve(struct wgetfile *finfo)
{
	FILE *f;
	char *token;

	if(strlen(finfo->host_file) > 1) {
		token = &finfo->host_file[2];
	} else {
		token = finfo->host_file;
	}

	fdprintf(finfo->sock, "%s\r\n", token);

	f = wget_open(finfo, NULL);
	if(!f)
		return 1;

	return generic_transfer(finfo, f, 0, 0);
}

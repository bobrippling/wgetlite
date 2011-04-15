#ifndef COOKIES_H
#define COOKIES_H

struct cookie
{
	char *host;
	char *nam, *val;
	struct cookie *next;
};

int  cookies_load(const char *fname);
void cookies_end(void);

struct cookie *cookies_get(const char *host);
void           cookies_free(struct cookie *, int free_vals);

#endif

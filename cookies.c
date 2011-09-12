#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>

#include "cookies.h"
#include "output.h"
#include "wgetlite.h"
#include "util.h"

static struct cookie *cookies = NULL;

#define NEW_COOKIE(x) \
	(x) = malloc(sizeof *(x)); \
	if(!(x)){ \
		output_perror("malloc()"); \
		goto bail; \
	} \
	memset(x, 0, sizeof *(x));


int cookies_load(const char *fname)
{
#define MAL_LINE(s) \
	{ \
		/*fputs(s "\n", stderr); */ \
		continue; \
	}

	FILE *f = fopen(fname, "r");
	char buffer[256];
	struct cookie **cur_cookie = &cookies;

	if(!f){
		output_err(OUT_WARN, "Couldn't load cookie file \"%s\"", fname);
		return 1;
	}

	while(fgets(buffer, sizeof buffer, f)){
		struct cookie *c;
		char *iter;
		int i;

		if((iter = strchr(buffer, '\n')))
			*iter = '\0';

		iter = strchr(buffer, '\t');
		if(iter){
			char *host, *nam, *val;

			char *prev;
			*iter = '\0';
			host = buffer;
			if(!*host)
				MAL_LINE("Empty host");

			for(i = 0; i < 5 && iter; i++){
				prev = iter + 1;
				iter = strchr(prev, '\t');
			}

			if(iter){
				nam = prev;

				iter = strchr(nam, '\t');

				if(iter){
					/* val */
					*iter = '\0';
					val = iter + 1;

					NEW_COOKIE(*cur_cookie);
					c = *cur_cookie;
					c->host = xstrdup(host);
					c->nam = xstrdup(nam);
					c->val = xstrdup(val);
				}else{
					MAL_LINE("No value");
				}
			}else{
				MAL_LINE("No name");
			}
		}else{
			MAL_LINE("No host");
		}

		cur_cookie = &c->next;
	}

	if(ferror(f)){
		output_perror("read()");
		goto bail;
	}

	fclose(f);
	return 0;

bail:
	fclose(f);
	cookies_free(cookies, 1);
	return 1;
}

void cookies_end(void)
{
	cookies_free(cookies, 1);
}


struct cookie *cookies_get(const char *host)
{
	struct cookie *c, *ret, *iter;
	char *qualhost = alloca(strlen(host) + 2);

	sprintf(qualhost, ".%s", host);

	NEW_COOKIE(ret);
	iter = ret;

	for(c = cookies; c; c = c->next)
		if(strfin(qualhost, c->host)){
			memcpy(iter, c, sizeof *iter);
			NEW_COOKIE(iter->next);
			iter = iter->next;
		}


	if(ret->nam){
		/* last one is empty */
		for(c = ret; c->next && c->next->next; c = c->next);
		cookies_free(c->next, 0);
		c->next = NULL;

		return ret;
	}

bail:
	cookies_free(ret, 0);
	return NULL;
}

void cookies_free(struct cookie *c, int vals)
{
	struct cookie *o;
	while(c){
		o = c->next;
		if(vals){
			free(c->nam);
			free(c->val);
			free(c->host);
		}
		free(c);
		c = o;
	}
}

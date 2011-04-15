#ifndef UTIL_H
#define UTIL_H

/* general stuff */

long mstime(void);
char *strdup(const char *s);
FILE *fdup(FILE *f, char *mode);

/* socket util */

char *readline(int sock); /* in */
int fdprintf(int fd, const char *fmt, ...); /* out */
int dial(const char *host, const char *port); /* shake it all about */
/* do the hokey kokey and you turn around, that's what it's all about */
int generic_transfer(struct wgetfile *finfo,
		FILE *out, size_t len, size_t sofar);

const char *strfin(const char *s, const char *postfix);

#endif

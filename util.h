#ifndef UTIL_H
#define UTIL_H

/* general stuff */

long mstime(void);
char *strdup(const char *s);

/* socket util */

char *readline(int sock); /* in */
int fdprintf(int fd, const char *fmt, ...); /* out */
int dial(const char *host, int port); /* shake it all about */
/* do the hokey kokey and you turn around, that's what it's all about */
int generic_transfer(int sock, FILE *out, const char *fname, size_t len);

#endif

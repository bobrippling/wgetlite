#ifndef UTIL_H
#define UTIL_H

/* timing */
long mstime(void);


/* non-failing string functions */
char *xstrdup(const char *s);
char *xstrprintf(const char *, ...);

/* check if a string finishes with suffix */
const char *strfin(const char *s, const char *suffix);


/* socket util */
char *fdreadline(int sock);                     /* in */
int   fdprintf(int fd, const char *fmt, ...);   /* out */
int   dial(const char *host, const char *port); /* shake it all about */
/* do the hokey kokey and you turn around, that's what it's all about */

const char *bytes_to_str(long bits);

/* transfer however many bytes needed on a socket */
int generic_transfer(struct wgetfile *finfo, FILE *out, size_t len,
		size_t sofar, int closefd);


/* read len bytes from fd, checking for interrupts, etc */
int discard(int fd, int len);

#endif

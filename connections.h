#ifndef CONNECTIONS_H
#define CONNECTIONS_H

/*
 * This allows possible reuse of HTTP connections
 * (via keep-alive)
 * All connections are kept open until program exit
 * This should probably be fixed to detect timeouts
 */

int  connection_add(int fd, const char *host, const char *port);
int  connection_fd(         const char *host, const char *port);
void connection_fin(void);
void connection_close_fd(int fd);

/* this removes the connection if it's closed while reading */
void connection_discard_data(int fd, int len);

#endif

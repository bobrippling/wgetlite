#ifndef CONNECTIONS_H
#define CONNECTIONS_H

int  connection_add(int fd, const char *host, const char *port);
int  connection_fd(         const char *host, const char *port);
void connection_fin(void);
void connection_close_fd(int fd);
void connection_discard_data(int fd, int len);

#endif

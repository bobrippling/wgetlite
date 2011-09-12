#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "output.h"
#include "wgetlite.h"
#include "util.h"
#include "connections.h"

#define CLOSE(x) do if((x) != -1) close(x); while(0)

struct connection
{
	int fd;
	char *host;
	char *port;

	struct connection *next;
} *conns = NULL;


struct connection *connection_find_fd(int fd)
{
	struct connection *c;

	for(c = conns; c; c = c->next)
		if(c->fd != -1 && c->fd == fd)
			return c;
	return NULL;
}

struct connection *connection_find_addr(const char *host, const char *port)
{
	struct connection *c;

	for(c = conns; c; c = c->next)
		if(c->fd != -1 && !strcmp(c->host, host) && !strcmp(c->port, port))
			return c;
	return NULL;
}

int connection_fd(const char *host, const char *port)
{
	struct connection *c = connection_find_addr(host, port);

	if(c)
		return c->fd;
	return -1;
}

int connection_add(int fd, const char *host, const char *port)
{
	struct connection *c = malloc(sizeof(*c));
	if(!c){
		output_perror("malloc()");
		return 1;
	}

	c->fd = fd;
	c->host = xstrdup(host);
	c->port = xstrdup(port);

	c->next = conns;
	conns = c;

	return 0;
}

void connection_fin(void)
{
	struct connection *c, *next;
	for(c = conns; c; c = next){
		CLOSE(c->fd);
		free(c->host);
		free(c->port);
		next = c->next;
		free(c);
	}
}

/* not used externally */
static void connection_close_private(struct connection *c)
{
	CLOSE(c->fd);
	c->fd = -1;
}

void connection_close_fd(int fd)
{
	struct connection *c;

	c = connection_find_fd(fd);

	if(c)
		connection_close_private(c);
	else
		CLOSE(fd);
}

void connection_discard_data(int fd, int len)
{
	/* read len bytes from fd */
	if(discard(fd, len)){
		struct connection *c;
		const char *host, *port;

		c = connection_find_fd(fd);
		if(c){
			host = c->host;
			port = c->port;
			connection_close_private(c);
		}else{
			host = "unknown";
			port = "unknown";
			connection_close_fd(fd);
		}

		/* no idea how much to discard/discard failed, close the connection */
		output_err(OUT_WARN, "Closing reuse-connection to %s:%s", host, port);
	}
}

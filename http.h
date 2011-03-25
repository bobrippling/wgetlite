#ifndef HTTP_H
#define HTTP_H

int http_GET(int sock, const char *file, const char *host, FILE **out, size_t fpos);

#endif

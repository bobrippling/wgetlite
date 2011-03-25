#ifndef FTP_H
#define FTP_H

int ftp_RETR(int sock, const char *file, const char *host, FILE **out, size_t fpos);

#endif

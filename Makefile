CFLAGS = -Wall -Wextra -pedantic -g -std=c99 -D_POSIX_SOURCE

wgetlite: wgetlite.o http.o progress.o util.o file.o ftp.o

clean:
	rm -f *.o wgetlite

.PHONY: clean

file.o: file.c
ftp.o: ftp.c util.h
http.o: http.c http.h progress.h util.h wgetlite.h
progress.o: progress.c progress.h
util.o: util.c util.h progress.h
wgetlite.o: wgetlite.c http.h file.h ftp.h wgetlite.h util.h

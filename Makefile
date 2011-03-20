CFLAGS = -Wall -Wextra -pedantic -g -std=c99

wgetlite: wgetlite.o http.o progress.o util.o file.o ftp.o

clean:
	rm -f *.o wgetlite

.PHONY: clean

file.o: file.c
ftp.o: ftp.c
http.o: http.c http.h progress.h util.h
progress.o: progress.c progress.h
util.o: util.c util.h
wgetlite.o: wgetlite.c http.h file.h

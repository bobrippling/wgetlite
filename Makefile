CFLAGS = -Wall -Wextra -pedantic -g -std=c99 -D_POSIX_SOURCE

wgetlite: wgetlite.o http.o progress.o util.o file.o ftp.o output.o

clean:
	rm -f *.o wgetlite

.PHONY: clean

file.o: file.c output.h
ftp.o: ftp.c util.h output.h
http.o: http.c http.h wgetlite.h progress.h util.h output.h
output.o: output.c output.h util.h wgetlite.h
progress.o: progress.c progress.h
util.o: util.c progress.h util.h output.h
wgetlite.o: wgetlite.c http.h file.h ftp.h wgetlite.h output.h util.h

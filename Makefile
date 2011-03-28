CFLAGS = -Wall -Wextra -pedantic -g -std=c99 -D_POSIX_SOURCE

wgetlite: wgetlite.o http.o progress.o util.o file.o ftp.o output.o term.o

clean:
	rm -f *.o wgetlite

.PHONY: clean

file.o: file.c output.h file.h
ftp.o: ftp.c ftp.h util.h output.h wgetlite.h
http.o: http.c http.h progress.h util.h output.h wgetlite.h
output.o: output.c output.h util.h wgetlite.h
progress.o: progress.c progress.h output.h wgetlite.h term.h
term.o: term.c
util.o: util.c progress.h util.h output.h
wgetlite.o: wgetlite.c http.h file.h ftp.h output.h util.h term.h \
 wgetlite.h

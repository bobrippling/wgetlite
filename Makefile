CFLAGS = -Wall -Wextra -pedantic -g -std=c99 -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200809L

wgetlite: main.o wgetlite.o http.o progress.o util.o \
	ftp.o output.o term.o cookies.o

clean:
	rm -f *.o wgetlite

.PHONY: clean

cookies.o: cookies.c cookies.h output.h wgetlite.h util.h
ftp.o: ftp.c output.h wgetlite.h ftp.h util.h
http.o: http.c output.h wgetlite.h util.h http.h progress.h cookies.h
main.o: main.c output.h wgetlite.h term.h cookies.h
output.o: output.c output.h wgetlite.h util.h
progress.o: progress.c progress.h output.h wgetlite.h term.h
term.o: term.c
util.o: util.c progress.h output.h wgetlite.h util.h
wgetlite.o: wgetlite.c output.h wgetlite.h http.h ftp.h util.h term.h

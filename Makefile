# to compile on FreeBSD, comment out all CFLAGS
CFLAGS  = -Wall -Wextra -pedantic -g -std=c99 -D_POSIX_C_SOURCE=200809L
VERSION = 1.1

OBJ = main.o wgetlite.o http.o progress.o util.o \
	ftp.o output.o term.o cookies.o connections.o \
	gopher.o

include config.mk

wgetlite: ${OBJ}
	${CC} -o $@ ${OBJ}

install: wgetlite
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f wgetlite ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/wgetlite

	mkdir -p ${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < wgetlite.1 > ${MANPREFIX}/man1/wgetlite.1
	@chmod 644 ${MANPREFIX}/man1/wgetlite.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/wgetlite
	rm -f ${DESTDIR}${MANPREFIX}/man1/wgetlite.1

clean:
	rm -f *.o wgetlite

.PHONY: clean

connections.o: connections.c output.h wgetlite.h util.h connections.h
cookies.o: cookies.c cookies.h output.h wgetlite.h util.h
ftp.o: ftp.c output.h wgetlite.h ftp.h util.h
http.o: http.c output.h wgetlite.h util.h http.h progress.h cookies.h \
 connections.h
main.o: main.c output.h wgetlite.h term.h cookies.h connections.h
output.o: output.c output.h wgetlite.h util.h
progress.o: progress.c progress.h output.h wgetlite.h term.h
term.o: term.c
util.o: util.c progress.h output.h wgetlite.h util.h connections.h
wgetlite.o: wgetlite.c output.h wgetlite.h http.h ftp.h util.h term.h \
 connections.h cookies.h

CFLAGS = -Wall -Wextra -pedantic -g

wgetlite: wgetlite.o http.o progress.o util.o file.o

clean:
	rm -f *.o wgetlite

.PHONY: clean

file.o: file.c
http.o: http.c http.h progress.h util.h
progress.o: progress.c progress.h
util.o: util.c util.h
wgetlite.o: wgetlite.c http.h file.h

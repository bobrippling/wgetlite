#!/bin/sh

WL=./wgetlite
set -e

etest(){
	printf '\e[1;34m%s\e[m\n' "$@"
}

usage(){
	echo "Usage: $0 [http|ftp]" >&2
	exit 1
}

http(){
	etest 1
	$WL -vvv -O /dev/null http://www.google.com/

	etest 2
	$WL -U '' -O- http://www.google.com:80/ > /dev/null

	etest 3
	$WL -c -O/dev/stderr -o /dev/stdout http://www.google.com:80/ 2> /dev/null

	etest 4
	$WL -O/dev/null google.co.uk

	etest 5
	$WL -O- www.google.com:80 > /dev/null
}

ftp(){
	# only need to test the protocol, rest is url parsing stuff

	f=/tmp/wgetlite_test

	trap "rm -f $f" EXIT

	etest 6
	$WL -O $f ftp://ftp.gnu.org/find.txt.gz

	len=$(expr $(stat -c '%s' $f) / 2)

	truncate -s$len $f

	etest 7
	$WL -c -O $f ftp://ftp.gnu.org/find.txt.gz
}

if [ $# -eq 1 ]
then
	case "$1" in
		ftp)
			ftp
			;;
		http)
			http
			;;
		*)
			usage
			;;
	esac
	exit
elif [ $# -ne 0 ]
then usage
fi

http
ftp

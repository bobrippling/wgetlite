#!/bin/sh

etest(){
	printf '\e[1;34m%s\e[1;0m\n' "$@"
}

WL=./wgetlite

set -e

# general tests

etest 1
$WL -O /dev/null http://www.google.com/

etest 2
$WL -O /dev/null http://www.google.com:80/

etest 3
$WL -O - http://www.google.com:80/

etest 4
$WL http://www.google.com/

etest 5
$WL http://www.google.com:80/

etest 6
$WL www.google.com/

etest 7
$WL www.google.com:80/

# only need to test the protocol, rest is parsing stuff

etest 8
$WL ftp://ftp.gnu.org/find.txt.gz

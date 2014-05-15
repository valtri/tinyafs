#! /bin/sh -e

if [ -z "$1" ]; then
	echo "Usage: $0 PRINCIPAL"
	exit 1
fi

echo -n "Password for '$1': "
stty -echo
read PASS
stty echo
clear

while true; do
	date
	echo "$PASS" | kinit $@
	aklog
	sleep 10800
done

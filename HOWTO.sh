#! /bin/bash

#
# 1) mysql setup
#
# CREATE DATABASE afs CHARACTER SET utf8 DEFAULT CHARACTER SET utf8 COLLATE utf8_bin DEFAULT COLLATE utf8_bin;
# GRANT ALL ON afs.* TO afs@localhost;
# USE afs;
# \. create.sql
#
# 2) list of volumes
#
# PERL5LIB=tools tools/listvol.pl | egrep -v '(^root\.afs|\.backup$)' > volumes.txt
#
# # optionally add some mountpoints: %s,^user\.\(.\)\(.*\),user\.\1\2^I/afs/zcu.cz/users/\1/\1\2,
# sed -i volumes.txt -e 's,^\(user\.\(.\)\(.*\)\),\1\t/afs/zcu.cz/users/\2/\2\3,'
#
# # check and correct all busy volumes
#
# 3) run environment
#    + system:administrators group
#    + ticket refresh needed
#
# pagsh
# screen
# ./refresh.sh LOGIN/root
#
# 4) scan
#

#example config file
cat << EOF >> pass
database=afs
servicedir=/afs/zcu.cz/users/v/valtri/home/service
cell=zcu.cz
EOF
chmod 0400 pass

./scan-sql -n 10 -l volumes.txt -c ./pass 2>&1 | tee log
./post-import-points.pl /afs/zcu.cz

exit 0

#
# check all rights due to LOGIN
#
./analyze.pl LOGIN

#
# SQL command to show only volumes with rights of LOGIN
#
SELECT DISTINCT volume FROM rights WHERE login='LOGIN' ORDER BY 1

#
# SQL command to show all numbered logins (needs to be cleared)
#
SELECT DISTINCT login FROM rights WHERE login LIKE '1%' OR login LIKE '2%' OR login LIKE '3%' OR login LIKE '4%' OR login LIKE '5%' OR login LIKE '6%' OR login LIKE '7%' OR login LIKE '8%' OR login LIKE '9%' ORDER BY login;

#
# list of volumes without mountpoints
# (only volumes with rights here, better is to compare volumes.txt and volumes
# table)
#
SELECT DISTINCT volume FROM rights WHERE volume NOT IN (SELECT volume FROM volumes);

#
# list of volumes without mountpoints, but without "subvolumes" mounted in them
#
SELECT DISTINCT pointvolume FROM mountpoints WHERE dir IS NULL AND pointvolume NOT IN (SELECT volume FROM mountpoints) ORDER BY 1;

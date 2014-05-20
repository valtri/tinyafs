#! /bin/bash

#
# 1) mysql setup
#
# CREATE DATABASE afs;
# GRANT ALL ON afs.* TO afs@localhost;
# USE afs;
# \. create.sql
#
# 2) list of volumes
#
# ./listvolumes.pl > volumes.txt
# # remove .backup
# # remove root.afs
# # optionally add some mountpoints: %s,^user\.\(.\)\(.*\),user\.\1\2^I/afs/zcu.cz/users/\1/\1\2,
# # check and correct all "volume_busy_"
#
# 3) run environment
#    + system:administrators group
#    + ticket refresh needed
#
# pagsh
# screen
# ./refresh.sh LOGIN/root
#
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

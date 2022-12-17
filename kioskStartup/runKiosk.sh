#!/bin/sh

antimicrox --hidden &
unclutter -idle 0.01 -root &
cd /home/swadgecomp/esp/Super-2023-Swadge-FW
./keepAlive.sh &
./kioskScript.sh
sleep 2
keepalivepid=$(pgrep "keepAlive")
unclutterpid=$(pidof unclutter)
echo Killing $unclutterpid
kill $keepalivepid
kill $unclutterpid
#unclutter -idle 30 -root
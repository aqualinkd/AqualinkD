#!/bin/bash
#

PROCESSNAME=aqualinkd
MYPID=`pidof $PROCESSNAME`

if [ $? -ne 0 ]; then
  MYPID=$(pidof "$PROCESSNAME-arm64")
  if [ $? -ne 0 ]; then
    MYPID=$(pidof "$PROCESSNAME-armhf")
  fi
fi

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

echo "===================";
echo "Process : $PROCESSNAME"
echo "PID     : $MYPID"
echo "-------------------"
Rss=`echo 0 $(cat /proc/$MYPID/smaps  | grep Rss | awk '{print $2}' | sed 's#^#+#') | bc;`
Shared=`echo 0 $(cat /proc/$MYPID/smaps  | grep Shared | awk '{print $2}' | sed 's#^#+#') | bc;`
Private=`echo 0 $(cat /proc/$MYPID/smaps  | grep Private | awk '{print $2}' | sed 's#^#+#') | bc;`
Swap=`echo 0 $(cat /proc/$MYPID/smaps  | grep Swap | awk '{print $2}' | sed 's#^#+#') | bc;`
Pss=`echo 0 $(cat /proc/$MYPID/smaps  | grep Pss | awk '{print $2}' | sed 's#^#+#') | bc;`

Mem=`echo "$Rss + $Shared + $Private + $Swap + $Pss"|bc -l`

echo "Rss     " $Rss
echo "Shared  " $Shared
echo "Private " $Private
echo "Swap    " $Swap
echo "Pss     " $Pss
echo "===================";
echo "Mem     " $Mem
echo "===================";

ps -p $MYPID -o %cpu,%mem,cmd
echo "===================";

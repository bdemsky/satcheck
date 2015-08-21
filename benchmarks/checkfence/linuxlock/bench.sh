#!/bin/bash
source ../paths.sh

for i in 1 2 3 4 5 6 7 8 9 10 15 20 30 40 50 60
do 
echo size=$i
cat linuxrwlocksbig.c.in | sed s/PROBLEMSIZE/$i/ > linuxrwlocksbig.c
c2lsl.exe lin_harness.c lin_harness.lsl
time checkfence -i -a memmodel=sc lin_harness.lsl locktests.lsl >> runlog
echo With loop bounds
time checkfence -a memmodel=sc lin_harness.lsl locktests.lsl T0.prn >> runlog
done



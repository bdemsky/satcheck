#!/bin/bash
source ../paths.sh


#for i in 1 2 3 4 5 6 7 8 9 10 15
#do 
#echo bias set to 0x100
#echo size=$i
#cat llock.c.in | sed s/PROBLEMSIZE/$i/ |sed s/TUNELOCK/0x100/ > llock.c
#c2lsl.exe add_harness.c add_harness.lsl
#time checkfence -i -a memmodel=sc add_harness.lsl locktests.lsl >> runlog
#echo Hard loop bounds
#time checkfence -a memmodel=sc add_harness.lsl locktests.lsl T0.prn >> runlog
#done


for i in 1 2 3 4 5 6 7 8 9 10 15 20 30
do 
echo bias set to 100
echo size=$i
cat llock.c.in | sed s/PROBLEMSIZE/$i/ |sed s/TUNELOCK/100/ > llock.c
c2lsl.exe add_harness.c add_harness.lsl
time checkfence -i -a memmodel=sc add_harness.lsl locktests.lsl >> runlog
echo Hard loop bounds
time checkfence -a memmodel=sc add_harness.lsl locktests.lsl T0.prn >> runlog
done

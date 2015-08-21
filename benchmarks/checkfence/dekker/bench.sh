#!/bin/bash
source ../paths.sh

for i in 1 2 3 4 5 6 7 8 9 10
do 
echo size=$i
cat dekker-fences.c.in | sed s/PROBLEMSIZE/$i/ > dekker-fences.c
c2lsl.exe dekker-fences.c dekker-fences.lsl
time checkfence -i -a memmodel=sc dekker-fences.lsl dekkertests.lsl >> runlog
echo With loop bounds
time checkfence -a memmodel=sc dekker-fences.lsl dekkertests.lsl T0.prn >> runlog
done



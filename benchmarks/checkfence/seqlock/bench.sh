#!/bin/bash
source ../paths.sh
for i in 1 2 3 4 5 6 7 8 9 10 15 20 25 30 35 40
do 
echo size=$i
cat seqlock.c.in | sed s/PROBLEMSIZE/$i/ > seqlock.c
c2lsl.exe seqlock.c seqlock.lsl
time checkfence -i -a memmodel=sc seqlock.lsl seqtests.lsl >> runlog
echo Hard coded loop bound
time checkfence -a memmodel=sc seqlock.lsl seqtests.lsl T0.prn >> runlog
done

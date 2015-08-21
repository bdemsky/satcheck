#!/bin/bash
source ../paths.sh

for i in 1 2 3 4 5 6 7 8 9
do 
echo size=$i
cat msn_harness.c.in | sed s/PROBLEMSIZE/$i/ > msn_harness.c
c2lsl.exe msn_harness.c msn_harness.lsl
time checkfence -i -a memmodel=sc msn_harness.lsl tests.lsl >> runlog
echo With loop bounds
time checkfence -a memmodel=sc msn_harness.lsl tests.lsl T0.prn >> runlog
done




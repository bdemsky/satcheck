#!/bin/bash
source ../path.sh
for i in 1 2 3 4
do
echo size= $i
cat seqlock.c.in | sed s/PROBLEMSIZE/$i/ > seqlock.c
gcc seqlock.c -O3 -I$CDSCHECKERDIR/include -L$CDSCHECKERDIR -lmodel -o seqlock.o
export LD_LIBRARY_PATH=$CDSCHECKERDIR
time ./seqlock.o -Y
done
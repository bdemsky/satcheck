#!/bin/bash
source ../path.sh
for i in 1 2 3 4 5 6 7 8
do
echo size= $i
cat ms-queue_simple.c.in | sed s/PROBLEMSIZE/$i/ > ms-queue_simple.c
gcc ms-queue_simple.c -O3 -I$CDSCHECKERDIR/include -L$CDSCHECKERDIR -lmodel -o ms-queue_simple.o
export LD_LIBRARY_PATH=$CDSCHECKERDIR
time ./ms-queue_simple.o
done

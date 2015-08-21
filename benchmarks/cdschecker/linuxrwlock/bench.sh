#!/bin/bash
source ../path.sh
for i in 1 2 3 4 5 6 7
do
echo size= $i
cat linuxrwlocks.c.in | sed s/PROBLEMSIZE/$i/ > linuxrwlocks.c
gcc linuxrwlocks.c -O3 -I$CDSCHECKERDIR/include -L$CDSCHECKERDIR -lmodel -o linuxrwlocks.o
export LD_LIBRARY_PATH=$CDSCHECKERDIR
time ./linuxrwlocks.o 
done
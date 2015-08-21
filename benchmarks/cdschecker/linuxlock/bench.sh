#!/bin/bash
source ../path.sh
for i in 1 2 3 4 5 6 7 8 9
do
echo size= $i
cat linuxlocks.c.in | sed s/PROBLEMSIZE/$i/ > linuxlocks.c
gcc linuxlocks.c -O3 -I$CDSCHECKERDIR/include -L$CDSCHECKERDIR -lmodel -o linuxlocks.o
export LD_LIBRARY_PATH=$CDSCHECKERDIR
time ./linuxlocks.o 
done
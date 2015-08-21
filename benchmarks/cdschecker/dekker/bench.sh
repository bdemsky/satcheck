#!/bin/bash
source ../path.sh
for i in 1 2 3 4 5 6
do
echo size= $i
cat dekker-fences.cc.in | sed s/PROBLEMSIZE/$i/ > dekker-fences.cc
g++ dekker-fences.cc -O3 -I$CDSCHECKERDIR/include -L$CDSCHECKERDIR -lmodel -o dekker-fences.o
export LD_LIBRARY_PATH=$CDSCHECKERDIR
time ./dekker-fences.o -Y
done

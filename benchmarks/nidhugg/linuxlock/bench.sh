#!/bin/bash
source ../path.sh
export PATH=$PATH:$NIDHUGG/bin/

for i in 1 2 3 4 5 6 7 8 9
do
echo size= $i
java -cp .. transform $i linuxlocks.c.in linuxlocks.cc
time nidhuggc -O3 -std=c++11 -- -sc ./linuxlocks.cc
done

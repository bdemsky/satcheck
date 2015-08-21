#!/bin/bash
source ../path.sh
export PATH=$PATH:$NIDHUGG/bin/
for i in 1 2 3 4 5 6 7
do
echo size= $i
java -cp .. transform $i linuxrwlocks.c.in linuxrwlocks.cc
time nidhuggc -O3 -std=c++11 -- -sc ./linuxrwlocks.cc
done

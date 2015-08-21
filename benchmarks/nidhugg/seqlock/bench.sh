#!/bin/bash
source ../path.sh
export PATH=$PATH:$NIDHUGG/bin/

for i in 1 2 3
do
echo size= $i
java -cp .. transform $i seqlock.c.in seqlock.cc
time nidhuggc -O3 -std=c++11 -- -sc ./seqlock.cc
done

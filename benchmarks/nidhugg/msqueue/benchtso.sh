#!/bin/bash
source ../path.sh
export PATH=$PATH:$NIDHUGG/bin/

for i in 1 2 3 4 5 6 7 8
do
echo size= $i
java -cp .. transform $i ms-queue_simple.c.in ms-queue_simple.cc
time nidhuggc -O3 -std=c++11 -- -tso ./ms-queue_simple.cc
done

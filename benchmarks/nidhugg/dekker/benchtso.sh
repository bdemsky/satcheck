#!/bin/bash

# 4 doesn't work because it takes too long
source ../path.sh
export PATH=$PATH:$NIDHUGG/bin/
for i in 1 2 3 4 5 6
do
echo size= $i
java -cp .. transform $i dekker-fences.cc.in dekker-fences.cc
time nidhuggc -O3 -std=c++11 -- -tso ./dekker-fences.cc
done

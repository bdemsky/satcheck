#!/bin/sh
. ../path.sh
. ./benchmark-config.sh
echo $MC2DIR/clang/build/add_mc2_annotations ${NAME}_unannotated.cc -- -DPROBLEMSIZE=1 -I/usr/include -I$CLANG_INCLUDE_DIR -I$MC2DIR/include
$MC2DIR/clang/build/add_mc2_annotations ${NAME}_unannotated.cc -- -DPROBLEMSIZE=1 -I/usr/include -I$CLANG_INCLUDE_DIR -I$MC2DIR/include > ${NAME}.cc

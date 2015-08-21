#!/bin/bash
source ../path.sh
source benchmark-config.sh
export PATH=$MC2DIR:$PATH
/bin/rm -f log logall
if [ $RUN_FRONTEND == true ]; then
 ./compile.sh
fi
for i in $SIZES_TSO ;
do
echo size= $i >> log
gcc ${NAME}.cc -DPROBLEMSIZE=$i -O3 -I$MC2DIR/include -L$MC2DIR -ltso_model -o ${NAME}
export LD_LIBRARY_PATH=$MC2DIR
export DYLD_LIBRARY_PATH=$MC2DIR
(time ./${NAME} ${RUNTIME_FLAGS}) >> logall 2>> log
done

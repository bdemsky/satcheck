I. Getting Started

To run SATCheck, you need to have built a SAT Solver from SATlib.  To
obtain SATlib (from within the SATCheck directory):

git clone git://plrg.eecs.uci.edu/satlib.git

Then build a SAT solver in the SATlib distribution.  For example:

cd satlib/glucose-syrup/incremental
make
cp glucose ../../../sat_solver

Then you can build SATCheck.  Simply type make from within the main
SATCheck directory.

One might find the options in config.h useful.


II. Printing Event Graphs

Uncomment DUMP_EVENT_GRAPHS in config.h

Make clean and the remake the system.

Run the model checker.

Use dot (from graphviz) to convert the outputted dot files into the
format of your choice.

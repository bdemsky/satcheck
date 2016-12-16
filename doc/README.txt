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

III. Using the debugger (gdb).

You can launch the debugger by passing in the gdb option to run.sh (or
you can lok at run.sh to see how to launch the model checker from
within a debugger).  Since we trap SIGSEGV to detect pages that we
need to update, you have to disable stopping on SIGSEGV.  To do this,
use the following commands:

To run inside MacOS under gdb you need (lldb won't work as far as I know):
set dont-handle-bad-access 1 (sometime not necessary)
handle SIGBUS nostop noprint

To run in Linux under gdb, use:
handle SIGSEGV nostop noprint

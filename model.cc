/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "model.h"
#include "snapshot-interface.h"
#include "mcschedule.h"
#include "output.h"
#include "planner.h"

MC * model;

MC::MC(struct model_params params) :
	params(params),
	execution(new MCExecution())
{
}

MC::~MC() {
	delete execution;
}

/** Wrapper to run the user's main function, with appropriate arguments */
void user_main_wrapper(void *) {
	user_main(model->params.argc, model->params.argv);
}

/** Implements the main loop for model checking test case 
 */
void MC::check() {
	snapshot_record(0);
	do {
		run_execution();
		execution->get_planner()->plan();
		print_program_output();
#if SUPPORT_MOD_ORDER_DUMP
		execution->dumpExecution();
#endif
		execution->reset();
		snapshot_backtrack_before(0);
	} while(!execution->get_planner()->is_finished());
	dprintf(2, "Finished!\n");
}


/** Executes a single execution. */

void MC::run_execution() {
	thrd_t user_thread;
	Thread *t = new Thread(execution->get_next_tid(), &user_thread, &user_main_wrapper, NULL, NULL, NULL);
	execution->add_thread(t);
	execution->get_scheduler()->swap_threads(NULL, t);
}



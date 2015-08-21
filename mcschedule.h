/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef MCSCHEDULER_H
#define MCSCHEDULER_H
#include "classlist.h"
#include "mymemory.h"
#include "context.h"
#include "stl-model.h"

class WaitPair {
 public:
	WaitPair(ExecPoint * stoppoint, ExecPoint * waitpoint);
	~WaitPair();
	ExecPoint * getStop();
	ExecPoint * getWait();

	MEMALLOC;
 private:
	ExecPoint *stop_point;
	ExecPoint *wait_point;
};

class MCScheduler {
 public:
	MCScheduler(MCExecution * e);
	~MCScheduler();
	ucontext_t * get_system_context() { return &system_context; }
	void swap_threads(Thread * from, Thread *to);
	void reset();
	void check_preempt();
#ifdef TSO
	bool check_store_buffer(unsigned int tid);
#endif
	bool checkSet(unsigned int tid, ModelVector<ModelVector<WaitPair* > * > *set, unsigned int *setindex, ExecPoint *current);
	bool check_thread(unsigned int tid);
	void addWaitPair(EPRecord *stoprec, EPRecord *waitrec);
	void setNewFlag();

	MEMALLOC;
 private:
	unsigned int waitsetlen;
	ucontext_t system_context;
	MCExecution *execution;
	ModelVector<ModelVector<WaitPair* > * > *waitset;
	unsigned int * waitsetindex;
#ifdef TSO
	ModelVector<ModelVector<WaitPair* > * > *storeset;
	unsigned int * storesetindex;
#endif
	bool newbehavior;
};


#endif

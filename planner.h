/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef PLANNER_H
#define PLANNER_H
#include "classlist.h"
#include "hashset.h"
#include "mymemory.h"
#include "change.h"

typedef HashSet<MCChange *, intptr_t, 0, model_malloc, model_calloc, model_free, MCChangeHash, MCChangeEquals> ChangeHashSet;
typedef HSIterator<MCChange *, intptr_t, 0, model_malloc, model_calloc, model_free,MCChangeHash, MCChangeEquals> ChangeIterator;

enum PlanResult {NOSCHEDULE, SCHEDULED};

class Planner {
public:
	Planner(MCExecution * e);
	~Planner();
	bool is_finished();
	void plan();
	void addChange(MCChange *change);
	void processChanges();
	void processEquals(MCChange * change);
	void processFunction(MCChange * change);
	void processRMW(MCChange * change);
	void processLoad(MCChange * change);
	void processStore(MCChange * change);
	bool checkConstGraph(EPRecord *record, uint64_t val);
	void registerValue(EPRecord *record, uint64_t val, unsigned int index);
	ConstGen * getConstGen() {return cgen;}

	MEMALLOC;

private:
	void registerBranchValue(EPRecord *record, uint64_t val, unsigned int index);
	void registerLoadValue(EPRecord *record, uint64_t val, unsigned int index);
	void registerRMWValue(EPRecord *record, uint64_t val, unsigned int index);
	void registerStoreValue(EPRecord *record, uint64_t val, unsigned int index);
	void registerFunctionValue(EPRecord *record, uint64_t val, unsigned int index);
	void registerEqualsValue(EPRecord *record, uint64_t val, unsigned int index);
	void doRMWRFChange(EPRecord *record, uint64_t readval);
	void doRMWBaseChange(EPRecord *record, uint64_t baseval);
	void doRMWOldValChange(EPRecord *record);
	void doRMWNewAddrChange(EPRecord *record, uint64_t addr);



	void processNewStoreAddress(MCChange *change);
	void processNewStoreValue(MCChange *change);
	void processNewReturnValue(MCChange *change);
	void processNewLoadAddress(MCChange *change);
	MCExecution * e;
	ChangeHashSet * changeset;
	ChangeHashSet * completedset;
	ConstGen *cgen;
	bool finished;
};
#endif

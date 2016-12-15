/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "planner.h"
#include "mcexecution.h"
#include "mcschedule.h"
#include "mcutil.h"
#include "constgen.h"
#include "change.h"

Planner::Planner(MCExecution * execution) :
	e(execution),
	changeset(new ChangeHashSet()),
	completedset(new ChangeHashSet()),
	cgen(new ConstGen(execution)),
	finished(false)
{
}

Planner::~Planner() {
	delete changeset;
	delete completedset;
	delete cgen;
}

bool Planner::is_finished() {
	return finished;
}

void Planner::plan() {
	DEBUG("Planning\n");
	e->get_scheduler()->reset();

	if (!cgen->canReuseEncoding()) {
		processChanges();
		cgen->reset();
		if (cgen->process())
			finished=true;
	}
}

void Planner::addChange(MCChange *change) {
	if (!changeset->add(change)) {
		delete change;
	}
}

void Planner::processChanges() {
	while(!changeset->isEmpty()) {
		MCChange *change=changeset->getFirstKey();
		if (change==NULL)
			break;
		changeset->remove(change);
		if (completedset->contains(change)) {
			delete change;
			continue;
		}
		if (change->isFunction()) {
			processFunction(change);
		} else if (change->isEquals()) {
			processEquals(change);
		} else if (change->isLoad()) {
			processLoad(change);
		} else if (change->isRMW()) {
			processRMW(change);
		} else if (change->isStore()) {
			processStore(change);
		} else ASSERT(false);
		completedset->add(change);
	}

	ChangeIterator *cit=completedset->iterator();

	for(;cit->hasNext();) {
		MCChange *change=cit->next();
		cit->remove();
		delete change;
	}
	delete cit;
}

void Planner::processFunction(MCChange * change) {
	processNewReturnValue(change);
}

void Planner::processEquals(MCChange * change) {
	processNewReturnValue(change);
}

void Planner::processRMW(MCChange * change) {
	switch(change->getIndex()) {
	case VC_ADDRINDEX:
		processNewStoreAddress(change);
		break;
	case VC_RMWOUTINDEX:
		processNewStoreValue(change);
		break;
	case VC_VALOUTINDEX:
		//Return value of the RMW action
		processNewReturnValue(change);
		break;
	default:
		ASSERT(false);
	}
}

void Planner::processLoad(MCChange * change) {
	switch(change->getIndex()) {
	case VC_ADDRINDEX:
		processNewLoadAddress(change);
		break;
	case VC_VALOUTINDEX:
		processNewReturnValue(change);
		break;
	default:
		ASSERT(0);
	}
}

void Planner::processStore(MCChange * change) {
	switch(change->getIndex()) {
	case VC_ADDRINDEX:
		processNewStoreAddress(change);
		break;
	case VC_BASEINDEX:
		processNewStoreValue(change);
		break;
	default:
		ASSERT(0);
	}
}

/** This function propagate news values that a function or add
                operation may generate.
 */

void Planner::processNewReturnValue(MCChange *change) {
	EPRecord *record=change->getRecord();
	EPRecordSet *dependences=e->getDependences(record);
	if (dependences==NULL)
		return;
	EPRecordIterator *rit=dependences->iterator();
	while(rit->hasNext()) {
		struct RecordIntPair *deprecord=rit->next();
		registerValue(deprecord->record, change->getValue(), deprecord->index);
	}
	delete rit;
}

/** This function registers a new address for a load operation.  We
                iterate over all stores to that new address and grab their values
                and propagate them.
 */

void Planner::processNewLoadAddress(MCChange *change) {
	EPRecord *load=change->getRecord();
	void *addr=(void *)change->getValue();
	RecordSet *storeset=e->getStoreTable(addr);
	if (storeset == NULL)
		return;
	RecordIterator *rit=storeset->iterator();
	while(rit->hasNext()) {
		EPRecord *store=rit->next();
		if (e->compatibleStoreLoad(store, load)) {
			IntIterator * it=store->getStoreSet()->iterator();
			while(it->hasNext()) {
				uint64_t st_val=it->next();
				//should propagate value further
				//we should worry about redudant values...
				registerValue(load, st_val, VC_RFINDEX);
			}
			delete it;
		}
	}
	delete rit;
}

/** This function processes a new address for a store.  We push our
                values to all loads from that address. */

void Planner::processNewStoreAddress(MCChange *change) {
	EPRecord *store=change->getRecord();
	void *addr=(void *)change->getValue();
	RecordSet *rset=e->getLoadTable(addr);
	if (rset == NULL)
		return;
	RecordIterator *rit=rset->iterator();
	IntHashSet *valset=store->getStoreSet();
	while(rit->hasNext()) {
		EPRecord *load=rit->next();
		if (e->compatibleStoreLoad(store, load)) {
			//iterate through all values
			IntIterator *iit=valset->iterator();
			while(iit->hasNext()) {
				uint64_t val=iit->next();
				registerValue(load, val, VC_RFINDEX);
			}
			delete iit;
		}
	}
	delete rit;
}


/** This function pushes a new store value to all loads that share an
                address. */

void Planner::processNewStoreValue(MCChange *change) {
	EPRecord *store=change->getRecord();
	uint64_t val=change->getValue();
	IntHashSet *addrset=store->getSet(VC_ADDRINDEX);
	IntIterator *ait=addrset->iterator();
	while(ait->hasNext()) {
		void *addr=(void*)ait->next();
		RecordSet *rset=e->getLoadTable(addr);
		if (rset == NULL)
			continue;
		RecordIterator *rit=rset->iterator();
		while(rit->hasNext()) {
			EPRecord *load=rit->next();
			if (e->compatibleStoreLoad(store, load)) {
				registerValue(load, val, VC_RFINDEX);
			}
		}
		delete rit;
	}
	delete ait;
}


void Planner::registerValue(EPRecord *record, uint64_t val, unsigned int index) {
	switch(record->getType()) {
	case LOAD:
		registerLoadValue(record, val, index);
		break;
	case RMW:
		registerRMWValue(record, val, index);
		break;
	case STORE:
		registerStoreValue(record, val, index);
		break;
	case FUNCTION:
		registerFunctionValue(record, val, index);
		break;
	case EQUALS:
		registerEqualsValue(record, val, index);
		break;
	case BRANCHDIR:
		registerBranchValue(record, val, index);
		break;
	case MERGE:
		break;
	default:
		model_print("Unrecognized event %u\n",record->getType());
	}
}

void Planner::registerBranchValue(EPRecord *record, uint64_t val, unsigned int index) {
	record->getSet(index)->add(val);
}

void Planner::registerLoadValue(EPRecord *record, uint64_t val, unsigned int index) {
	if (index==VC_ADDRINDEX)
		val+=record->getOffset();

	bool is_new=record->getSet(index)->add(val);
	if (is_new) {
		switch(index) {
		case VC_ADDRINDEX: {
			e->addLoadTable((void *)val, record);
			MCChange *change=new MCChange(record, val, VC_ADDRINDEX);
			addChange(change);
			break;
		}
		case VC_RFINDEX: {
			//New value we can read...Push it...
			MCChange *change=new MCChange(record, val, VC_VALOUTINDEX);
			addChange(change);
			break;
		}
		default:
			ASSERT(0);
		}
	}
}


void Planner::registerRMWValue(EPRecord *record, uint64_t val, unsigned int index) {
	if (index==VC_ADDRINDEX)
		val+=record->getOffset();

	bool is_new=record->getSet(index)->add(val);
	if (is_new) {
		switch(index) {
		case VC_ADDRINDEX:
			doRMWNewAddrChange(record, val);
			break;
		case VC_RFINDEX:
			doRMWRFChange(record, val);
			break;
		case VC_BASEINDEX:
			doRMWBaseChange(record, val);
			break;
		case VC_OLDVALCASINDEX:
			ASSERT(record->getOp()==CAS);
			doRMWOldValChange(record);
			break;
		default:
			ASSERT(0);
		}
	}
}

void Planner::doRMWNewAddrChange(EPRecord *record, uint64_t val) {
	e->addLoadTable((void *)val, record);
	e->addStoreTable((void *)val, record);

	//propagate our value to new loads
	MCChange * change=new MCChange(record, val, VC_ADDRINDEX);
	addChange(change);

	//look at new stores and update our read from set
	RecordSet *storeset=e->getStoreTable((void *)val);
	RecordIterator *rit=storeset->iterator();
	while(rit->hasNext()) {
		EPRecord *store=rit->next();

		if (e->compatibleStoreLoad(store, record)) {
			IntIterator * it=store->getStoreSet()->iterator();
			while(it->hasNext()) {
				uint64_t st_val=it->next();
				//should propagate value further
				//we should worry about redudant values...
				registerRMWValue(record, st_val, VC_RFINDEX);
			}
			delete it;
		}
	}
	delete rit;
}

void Planner::doRMWRFChange(EPRecord *record, uint64_t readval) {
	//Register the new value we might return
	MCChange *change=new MCChange(record, readval, VC_VALOUTINDEX);
	addChange(change);

	if (record->getOp()==CAS) {
		//Register the new value we might store if we are a CAS
		bool is_new=record->getStoreSet()->add(readval);
		if (is_new) {
			MCChange *change=new MCChange(record, readval, VC_RMWOUTINDEX);
			addChange(change);
		}
	}
}

void Planner::doRMWBaseChange(EPRecord *record, uint64_t baseval) {
	if (record->getOp()==CAS) {
		//Just push the value as though it is our output
		bool is_new=record->getStoreSet()->add(baseval);
		if (is_new) {
			MCChange *change=new MCChange(record, baseval, VC_RMWOUTINDEX);
			addChange(change);
		}
	} else if (record->getOp()==ADD) {
		//Tricky here because we could create an infinite propagation...
		//So we drop it...
		//TODO: HANDLE THIS CASE
	} else if (record->getOp()==EXC) {
		//no need to propagate output
	} else {
		ASSERT(0);
	}
}

void Planner::doRMWOldValChange(EPRecord *record) {
	//Do nothing, no need to propagate old value...
}

void Planner::registerStoreValue(EPRecord *record, uint64_t val, unsigned int index) {
	if (index==VC_ADDRINDEX)
		val+=record->getOffset();

	bool is_new=record->getSet(index)->add(val);

	if (index==VC_ADDRINDEX) {
		if (is_new)
			e->addStoreTable((void *)val, record);
		MCChange * change=new MCChange(record, val, index);
		addChange(change);
	} else if (index==VC_BASEINDEX) {
		MCChange * change=new MCChange(record, val, index);
		addChange(change);
	} else model_print("ERROR in RSV\n");
}

void Planner::registerEqualsValue(EPRecord *record, uint64_t val, unsigned int index) {
	record->getSet(index)->add(val);
}

void Planner::registerFunctionValue(EPRecord *record, uint64_t val, unsigned int index) {
	bool newval=record->getSet(index)->add(val);

	if (record->getPhi()) {
		MCChange * change=new MCChange(record, val, VC_FUNCOUTINDEX);
		addChange(change);
		return;
	} else if (newval && record->isSharedGoals()) {
		CGoalIterator *cit=record->completedGoalSet()->iterator();
		while(cit->hasNext()) {
			CGoal *goal=cit->next();
			if (goal->getValue(index)==val) {
				e->evalGoal(record, goal);
			}
		}
		delete cit;
	}
}

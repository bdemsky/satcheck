/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "schedulebuilder.h"
#include "mcexecution.h"
#include "mcschedule.h"
#include "constgen.h"
#include "branchrecord.h"
#include "storeloadset.h"

ScheduleBuilder::ScheduleBuilder(MCExecution *_execution, ConstGen *cgen) :
	cg(cgen),
	execution(_execution),
	scheduler(execution->get_scheduler())
{
}

ScheduleBuilder::~ScheduleBuilder() {
}


void neatPrint(EPRecord *r, ConstGen *cgen, bool *satsolution) {
	r->print();
	switch(r->getType()) {
	case LOAD: {
		StoreLoadSet * sls=cgen->getStoreLoadSet(r);
		model_print("address=%p ",  sls->getAddressEncoding(cgen, r, satsolution));
		model_print("rd=%llu ", sls->getValueEncoding(cgen, r, satsolution));
	}
	break;
	case STORE: {
		StoreLoadSet * sls=cgen->getStoreLoadSet(r);
		model_print("address=%p ",  sls->getAddressEncoding(cgen, r, satsolution));
		model_print("wr=%llu ", sls->getValueEncoding(cgen, r, satsolution));
	}
	break;
	case RMW: {
		StoreLoadSet * sls=cgen->getStoreLoadSet(r);
		model_print("address=%p ",  sls->getAddressEncoding(cgen, r, satsolution));
		model_print("rd=%llu ", sls->getRMWRValueEncoding(cgen, r, satsolution));
		model_print("wr=%llu ", sls->getValueEncoding(cgen, r, satsolution));
	}
	break;
	default:
		;
	}
	model_print("\n");
}

void ScheduleBuilder::buildSchedule(bool * satsolution) {
	threads.push_back(execution->getEPRecord(MCID_FIRST));
	lastoperation.push_back(NULL);
#ifdef TSO
	stores.push_back(new SnapList<EPRecord *>());
	storelastoperation.push_back(NULL);
#endif
	while(true) {
		//Loop over threads...getting rid of non-load/store actions
		for(uint index=0;index<threads.size();index++) {
			EPRecord *record=threads[index];
			if (record==NULL)
				continue;
			EPRecord *next=processRecord(record, satsolution);
#ifdef TSO
			if (next != NULL) {

				if (next->getType()==STORE) {
					stores[index]->push_back(next);
					next=getNextRecord(next);
				} else if (next->getType()==FENCE) {
					if (!stores[index]->empty())
						scheduler->addWaitPair(next, stores[index]->back());
					next=getNextRecord(next);
				} else if (next->getType()==RMW) {
					if (!stores[index]->empty())
						scheduler->addWaitPair(next, stores[index]->back());
				}
			}
#endif
			if (next!=record) {
				threads[index]=next;
				index=index-1;
			}
		}
		//Now we have just memory operations, find the first one...make it go first
		EPRecord *earliest=NULL;
		for(uint index=0;index<threads.size();index++) {
			EPRecord *record=threads[index];

			if (record!=NULL && (earliest==NULL ||
													 cg->getOrder(record, earliest, satsolution))) {
				earliest=record;
			}
#ifdef TSO
			if (!stores[index]->empty()) {
				record=stores[index]->front();
				if (record!=NULL && (earliest==NULL ||
														 cg->getOrder(record, earliest, satsolution))) {
					earliest=record;
				}
			}
#endif
		}

		if (earliest == NULL)
			break;

		for(uint index=0;index<threads.size();index++) {
			EPRecord *record=threads[index];
			if (record==earliest) {
				//Update index in vector for current thread
				EPRecord *next=getNextRecord(record);
				threads[index]=next;
				lastoperation[index]=record;
			} else {
				EPRecord *last=lastoperation[index];
				if (last!=NULL) {
					scheduler->addWaitPair(earliest, last);
				}
			}
#ifdef TSO
			EPRecord *recstore;
			if (!stores[index]->empty() && (recstore=stores[index]->front())==earliest) {
				//Update index in vector for current thread
				stores[index]->pop_front();
				storelastoperation[index]=recstore;
			} else {
				EPRecord *last=storelastoperation[index];
				if (last!=NULL) {
					scheduler->addWaitPair(earliest, last);
				}
			}
#endif
		}
	}
}

EPRecord * ScheduleBuilder::getNextRecord(EPRecord *record) {
	EPRecord *next=record->getNextRecord();

	//If this branch exit isn't in the current model, we should not go
	//there...
	if (record->getType()==BRANCHDIR && next!=NULL) {
		BranchRecord *br=cg->getBranchRecord(record);
		if (!br->hasNextRecord())
			next=NULL;
	}

	if (next==NULL && record->getBranch()!=NULL) {
		EPValue * epbr=record->getBranch();
		EPRecord *branch=epbr->getRecord();
		if (branch!=NULL) {
			return getNextRecord(branch);
		}
	}
	return next;
}

EPRecord * ScheduleBuilder::processRecord(EPRecord *record, bool *satsolution) {
	switch(record->getType()) {
	case LOAD:
	case STORE:
	case RMW:
#ifdef TSO
	case FENCE:
#endif
		return record;
	case LOOPENTER: {
		return record->getChildRecord();
		break;
	}
	case BRANCHDIR: {
		BranchRecord *br=cg->getBranchRecord(record);
		int index=br->getBranchValue(satsolution);

		if (index>=0 && index < br->numDirections()) {
			EPValue *val=record->getValue(index);
			EPRecord *branchrecord=val->getFirstRecord();
			if (branchrecord!=NULL)
				return branchrecord;
		}
		//otherwise just go past the branch...we don't know what this code is going to do
		break;
	}
	case LOOPSTART:
	case MERGE:
	case ALLOC:
	case EQUALS:
	case FUNCTION:
		/* Continue executing */
		break;
	case THREADCREATE:
		/* Start next thread */
		threads.push_back(record->getChildRecord());
		lastoperation.push_back(NULL);
#ifdef TSO
		stores.push_back(new SnapList<EPRecord *>());
		storelastoperation.push_back(NULL);
#endif
		break;
	case THREADBEGIN:
		break;
	case THREADJOIN:
		break;
	case NONLOCALTRANS:
		break;
	case LOOPEXIT:
		break;
	case LABEL:
		break;
	case YIELD:
		break;
	default:
		ASSERT(0);
	}
	return getNextRecord(record);
}

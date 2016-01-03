/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "eprecord.h"
#include "change.h"
#include "model.h"
#include "planner.h"

EPRecord::EPRecord(EventType _event, ExecPoint *_execpoint, EPValue *_branch, uintptr_t _offset, unsigned int _numinputs, uint _len, bool _anyvalue) :
	valuevector(new ValueVector()),
	execpoint(_execpoint),
	completed(NULL),
	branch(_branch),
	setarray((IntHashSet **)model_malloc(sizeof(IntHashSet *)*(_numinputs+VC_BASEINDEX))),
	nextRecord(NULL),
	childRecord(NULL),
	philooptable(NULL),
	offset(_offset),
	event(_event),
	jointhread(THREAD_ID_T_NONE),
	numinputs(_numinputs+VC_BASEINDEX),
	len(_len),
	anyvalue(_anyvalue),
	phi(false),
	loopphi(false),
	size(0),
	func_shares_goalset(false),
	ptr(NULL)
{
	for(unsigned int i=0;i<numinputs;i++)
		setarray[i]=new IntHashSet();
}

EPRecord::~EPRecord() {
	if (!func_shares_goalset && (completed != NULL))
		delete completed;
	delete valuevector;
	if (completed != NULL)
		delete completed;
	if (philooptable!=NULL) {
		EPRecordIDIterator *rit=philooptable->iterator();
		while(rit->hasNext()) {
			struct RecordIDPair *val=rit->next();
			model_free(val);
		}
		delete rit;
		delete philooptable;
	}
	model_free(setarray);
}

const char * eventToStr(EventType event) {
	switch(event) {
	case ALLOC:
		return "alloc";
	case LOAD:
		return "load";
	case RMW:
		return "rmw";
	case STORE:
		return "store";
	case BRANCHDIR:
		return "br_dir";
	case MERGE:
		return "merge";
	case FUNCTION:
		return "func";
	case EQUALS:
		return "equals";
	case THREADCREATE:
		return "thr_create";
	case THREADBEGIN:
		return "thr_begin";
	case THREADJOIN:
		return "join";
	case YIELD:
		return "yield";
	case NONLOCALTRANS:
		return "nonlocal";
	case LOOPEXIT:
		return "loopexit";
	case LABEL:
		return "label";
	case LOOPENTER:
		return "loopenter";
	case LOOPSTART:
		return "loopstart";
	case FENCE:
		return "fence";
	default:
		return "unknown";
	}
}

IntHashSet * EPRecord::getReturnValueSet() {
	switch(event) {
	case FUNCTION:
	case EQUALS:
		return getSet(VC_FUNCOUTINDEX);
	case RMW:
		return getSet(VC_RMWOUTINDEX);
	case LOAD:
		return getSet(VC_RFINDEX);
	case ALLOC:
	case BRANCHDIR:
	case MERGE:
	case STORE:
	case THREADCREATE:
	case THREADBEGIN:
	case THREADJOIN:
	case YIELD:
	case FENCE:
	case NONLOCALTRANS:
	case LOOPEXIT:
	case LABEL:
	case LOOPENTER:
	case LOOPSTART:
		ASSERT(false);
		return NULL;
	default:
		ASSERT(false);
		return NULL;
	}
}

void EPRecord::print(int f) {
	if (event==RMW) {
		if (op==ADD)
			dprintf(f, "add");
		else if (op==EXC)
			dprintf(f, "exc");
		else
			dprintf(f, "cas");
	} else
		dprintf(f, "%s",eventToStr(event));
	IntHashSet *i=NULL;
	switch(event) {
	case LOAD:
		i=setarray[VC_RFINDEX];
		break;
	case STORE:
		i=setarray[VC_BASEINDEX];
		break;
	case RMW:
		i=setarray[VC_RMWOUTINDEX];
		break;
	case FUNCTION: {
		if (!getPhi()) {
			CGoalIterator *cit=completed->iterator();
			dprintf(f, "{");
			while(cit->hasNext()) {
				CGoal *goal=cit->next();
				dprintf(f,"(");
				for(uint i=0;i<getNumFuncInputs();i++) {
					dprintf(f,"%lu ", goal->getValue(i+VC_BASEINDEX));
				}
				dprintf(f,"=> %lu)", goal->getOutput());
			}
			delete cit;
		}
		break;
	}
	default:
		;
	}
	if (i!=NULL) {
		IntIterator *it=i->iterator();
		dprintf(f, "{");

		while(it->hasNext()) {
			dprintf(f, "%lu ", it->next());
		}
		dprintf(f, "}");
		delete it;
	}

	for(uint i=0;i<numinputs;i++) {
		IntIterator *it=setarray[i]->iterator();
		dprintf(f,"{");
		while(it->hasNext()) {
			uint64_t v=it->next();
			dprintf(f,"%lu ", v);
		}
		dprintf(f,"}");
		delete it;
	}

	
	execpoint->print(f);
}

void EPRecord::print() {
	model_print("%s",eventToStr(event));
	execpoint->print();
	model_print(" CR=%p ", this);
	model_print(" NR=%p", nextRecord);
}

bool EPRecord::hasAddr(const void * addr) {
	return getSet(VC_ADDRINDEX)->contains((uintptr_t)addr);
}

int EPRecord::getIndex(EPValue *val) {
	for(unsigned int i=0;i<valuevector->size();i++) {
		EPValue *epv=(*valuevector)[i];
		if (epv==val)
			return i;
	}
	return -1;
}

int EPRecord::getIndex(uint64_t val) {
	for(unsigned int i=0;i<valuevector->size();i++) {
		EPValue *epv=(*valuevector)[i];
		if (epv->getValue()==val)
			return i;
	}
	return -1;
}

bool EPRecord::hasValue(uint64_t val) {
	for(unsigned int i=0;i<valuevector->size();i++) {
		EPValue *epv=(*valuevector)[i];
		if (epv->getValue()==val)
			return true;
	}
	return false;
}

bool compatibleStoreLoad(EPRecord *store, EPRecord *load) {
	return true;
}

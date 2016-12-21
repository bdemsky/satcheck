/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "mcexecution.h"
#include "mcschedule.h"
#include "planner.h"
#include <stdarg.h>
#include "cgoal.h"
#include "change.h"
#include "mcutil.h"
#include "model.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "constgen.h"

MCExecution_snapshot::MCExecution_snapshot() :
	next_thread_id(0)
{
}

MCExecution_snapshot::~MCExecution_snapshot() {
}


MCExecution::MCExecution() :
	EPTable(new EPHashTable()),
	EPValueTable(new EPValueHashTable()),
	memtable(new MemHashTable()),
	storetable(new StoreLoadHashTable()),
	loadtable(new StoreLoadHashTable()),
	EPList(new SnapVector<EPValue *>()),
	ThreadList(new SnapVector<Thread *>()),
	ExecPointList(new SnapVector<ExecPoint *>()),
	CurrBranchList(new SnapVector<EPValue *>()),
	currexecpoint(NULL),
	scheduler(new MCScheduler(this)),
	planner(new Planner(this)),
	snap_fields(new MCExecution_snapshot()),
	curr_thread(NULL),
	currbranch(NULL),
	eprecorddep(new EPRecordDepHashTable()),
	revrecorddep(new RevDepHashTable()),
	alwaysexecuted(new ModelVector<EPRecord *>()),
	lastloopexit(new ModelVector<EPRecord *>()),
	joinvec(new ModelVector<EPRecord *>()),
	sharedgoals(new ModelVector<CGoalSet *>()),
	sharedfuncrecords(new ModelVector<RecordSet *>()),
	currid(MCID_INIT),
	schedule_graph(0)
{
	EPList->push_back(NULL);			//avoid using MCID of 0
#ifdef TSO
	storebuffer = new SnapVector<SnapList<EPValue *> *>();
#endif
}

MCExecution::~MCExecution() {
	reset();
	delete EPTable;
	delete EPValueTable;
	delete memtable;
	delete EPList;
	delete ThreadList;
	delete ExecPointList;
	delete scheduler;
	delete snap_fields;
	delete eprecorddep;
	delete revrecorddep;
	delete joinvec;
	delete alwaysexecuted;
	delete lastloopexit;
#ifdef TSO
	for(uint i=0;i<storebuffer->size();i++) {
		SnapList<EPValue *> *list=(*storebuffer)[i];
		if (list != NULL)
			delete list;
	}
	delete storebuffer;
#endif
	for(uint i=0;i<sharedgoals->size();i++) {
		CGoalSet *c=(*sharedgoals)[i];
		if (c != NULL)
			delete c;
	}
	delete sharedgoals;
	for(uint i=0;i<sharedfuncrecords->size();i++) {
		RecordSet *rs=(*sharedfuncrecords)[i];
		if (rs != NULL)
			delete rs;
	}
	delete sharedfuncrecords;
}

/** @brief Resets MCExecution object for next execution. */

void MCExecution::reset() {
	currid=MCID_INIT;
	currbranch=NULL;
	alwaysexecuted->clear();
}

void MCExecution::dumpExecution() {
	char buffer[128];
	sprintf(buffer, "exec%u.dot",schedule_graph);
	schedule_graph++;
	int file=open(buffer,O_WRONLY|O_TRUNC|O_CREAT, S_IRWXU);
	model_dprintf(file, "digraph execution {\n");
	EPRecord *last=NULL;
	for(uint i=0;i<EPList->size();i++) {
		EPValue *epv=(*EPList)[i];
		if (epv==NULL)
			continue;
		EPRecord *record=epv->getRecord();
		model_dprintf(file, "%lu[label=\"",(uintptr_t)record);
		record->print(file);
		model_dprintf(file, "\"];\n");
		if (last!=NULL)
			model_dprintf(file, "%lu->%lu;", (uintptr_t) last, (uintptr_t) record);
		last=record;
	}
	model_dprintf(file, "}\n");
	close(file);
}

/** @brief Records the MCID and returns the next MCID */

MCID MCExecution::loadMCID(MCID maddr, uintptr_t offset) {
	scheduler->check_preempt();
	id_addr=maddr;
	id_addr_offset=offset;
	return id_retval=getNextMCID();
}

/** @brief Records the address and value MCIDs. */

void MCExecution::storeMCID(MCID maddr, uintptr_t offset, MCID val) {
#ifndef TSO
	scheduler->check_preempt();
#endif
	id_addr=maddr;
	id_addr_offset=offset;
	id_value=val;
}

/** @brief Records MCIDs for a RMW. */

MCID MCExecution::nextRMW(MCID maddr, uintptr_t offset, MCID oldval, MCID valarg) {
	scheduler->check_preempt();
	id_addr=maddr;
	id_addr_offset=offset;
	id_oldvalue=oldval;
	id_value=valarg;
	id_retval=getNextMCID();
	return id_retval;
}

uint64_t dormwaction(enum atomicop op, void *addr, uint len, uint64_t currval, uint64_t oldval, uint64_t valarg, uint64_t *newval) {
	uint64_t retval;
	uint64_t memval;
	switch(op) {
	case ADD:
		retval=currval;
		memval=currval+valarg;
		break;
	case CAS:
		retval=currval;
		if (currval==oldval) {
			memval=valarg;
		} else {
			memval=currval;
		}
		break;
	case EXC:
		retval=currval;
		memval=valarg;
		break;
	default:
		ASSERT(false);
		return -1;
	}

	*newval=memval&getmask(len);

	if (op==ADD || (op==CAS && (currval==oldval)) || (op == EXC)) {
		switch(len) {
		case 1:
			*((uint8_t *)addr)=*newval;
			break;
		case 2:
			*((uint16_t *)addr)=*newval;
			break;
		case 4:
			*((uint32_t *)addr)=*newval;
			break;
		case 8:
			*((uint64_t *)addr)=*newval;
			break;
		}
	}
	return retval;
}

/** @brief Implement atomic RMW. */
uint64_t MCExecution::rmw(enum atomicop op, void *addr, uint len, uint64_t currval, uint64_t oldval, uint64_t valarg) {
	uint64_t newval;
	uint64_t retval=dormwaction(op, addr, len, currval, oldval, valarg, &newval);
	if (DBG_ENABLED()) {
		model_print("RMW %p oldval=%llu valarg=%llu retval=%llu", addr, oldval, valarg, retval);
		currexecpoint->print();
		model_print("\n");
	}

	int num_mcids=(op==ADD) ? 1 : 2;
	EPRecord * record=getOrCreateCurrRecord(RMW, NULL, id_addr_offset, VC_RMWOUTINDEX-VC_RFINDEX, len, false);
	record->setRMW(op);

	recordRMWChange(record, addr, valarg, oldval, newval);
	addLoadTable(addr, record);
	addStoreTable(addr, record);
	EPValue * epvalue=getEPValue(record, NULL, addr, retval, len);
	MCID vcrf=memtable->get(addr);
	MCID mcids[]={id_value, id_oldvalue};
	recordContext(epvalue, vcrf, id_addr, num_mcids, mcids);
	ASSERT(EPList->size()==id_retval);
	EPList->push_back(epvalue);
	memtable->put(addr, id_retval);
	currexecpoint->incrementTop();
	return retval;
}

/** @brief Gets an EPRecord for the current execution point.  */

EPRecord * MCExecution::getOrCreateCurrRecord(EventType et, bool * isNew, uintptr_t offset,  unsigned int numinputs, uint len, bool br_anyvalue) {
	EPRecord *record=EPTable->get(currexecpoint);
	if (isNew!=NULL)
		*isNew=(record==NULL);
	if (record==NULL) {
		ExecPoint * ep=new ExecPoint(currexecpoint);
		record=new EPRecord(et, ep, currbranch, offset, numinputs, len, br_anyvalue);
		if (et==THREADJOIN) {
			joinvec->push_back(record);
		}
		EPTable->put(ep, record);
	} else {
		ASSERT(record->getOffset()==offset);
	}
	if (currbranch==NULL) {
		uint tid=id_to_int(currexecpoint->get_tid());
		if (alwaysexecuted->size()<=tid) {
			alwaysexecuted->resize(tid+1);
		}
		if ((*alwaysexecuted)[tid]!=NULL && et != LABEL && et != LOOPSTART)
			(*alwaysexecuted)[tid]->setNextRecord(record);
		(*alwaysexecuted)[tid]=record;
	} else {
		if (currbranch->firstrecord==NULL)
			currbranch->firstrecord=record;
		if (currbranch->lastrecord!=NULL && et != LABEL && et != LOOPSTART)
			currbranch->lastrecord->setNextRecord(record);
		currbranch->lastrecord=record;
	}

	return record;
}

/** @brief Gets an EPValue for the current record and value. */

EPValue * MCExecution::getEPValue(EPRecord * record, bool * isNew,  const void *addr, uint64_t val, int len) {
	ExecPoint * ep=record->getEP();
	EPValue * epvalue=new EPValue(ep, record, addr, val,len);
	EPValue * curr=EPValueTable->get(epvalue);
	if (curr!=NULL) {
		if (isNew!=NULL)
			*isNew=false;
		delete epvalue;
	} else {
		if (isNew!=NULL)
			*isNew=true;
		EPValueTable->put(epvalue, epvalue);
		record->addEPValue(epvalue);
		curr=epvalue;
	}
	return curr;
}

/** @brief Lookup EPRecord for the MCID. */

EPRecord * MCExecution::getEPRecord(MCID id) {
	return (*EPList)[id]->getRecord();
}

/** @brief Lookup EPValue for the MCID. */

EPValue * MCExecution::getEPValue(MCID id) {
	return (*EPList)[id];
}

#ifdef TSO
bool MCExecution::isEmptyStoreBuffer(thread_id_t tid) {
	SnapList<EPValue *> * list=(*storebuffer)[id_to_int(tid)];
	return list->empty();
}

void MCExecution::doStore(thread_id_t tid) {
	SnapList<EPValue *> * list=(*storebuffer)[id_to_int(tid)];
	EPValue * epval=list->front();
	list->pop_front();
	if (DBG_ENABLED()) {
		model_print("tid = %d: ", tid);
	}
	doStore(epval);
}

void MCExecution::doStore(EPValue *epval) {
	const void *addr=epval->getAddr();
	uint64_t val=epval->getValue();
	int len=epval->getLen();
	if (DBG_ENABLED()) {
		model_print("flushing %d bytes *(%p) = %llu", len, addr, val);
		currexecpoint->print();
		model_print("\n");
	}
	switch(len) {
	case 1:
		(*(uint8_t *)addr) = (uint8_t)val;
		break;
	case 2:
		(*(uint16_t *)addr) = (uint16_t)val;
		break;
	case 4:
		(*(uint32_t *)addr) = (uint32_t)val;
		break;
	case 8:
		(*(uint64_t *)addr) = (uint64_t)val;
		break;
	}
}

void MCExecution::fence() {
	scheduler->check_preempt();
	getOrCreateCurrRecord(FENCE, NULL, 0, 0, 8, false);
	currexecpoint->incrementTop();
	uint tid=id_to_int(currexecpoint->get_tid());
	SnapList<EPValue *> * list=(*storebuffer)[tid];
	while(!list->empty()) {
		EPValue * epval=list->front();
		list->pop_front();
		doStore(epval);
	}
}
#endif

/** @brief EPValue is the corresponding epvalue object.
                For loads rf is the store we read from.
                For loads or stores, addr is the MCID for the provided address.
                numids is the number of MCID's we take in.
                We then list that number of MCIDs for everything we depend on.
 */

void MCExecution::recordContext(EPValue * epv, MCID rf, MCID addr, int numids, MCID *mcids) {
	EPRecord *currrecord=epv->getRecord();

	for(int i=0;i<numids;i++) {
		MCID id=mcids[i];
		if (id != MCID_NODEP) {
			EPRecord * epi=getEPRecord(id);
			addRecordDepTable(epi,currrecord, VC_BASEINDEX+i);
		}
	}
	if (rf != MCID_NODEP) {
		EPRecord *eprf=getEPRecord(rf);
		addRecordDepTable(eprf,currrecord, VC_RFINDEX);
	}
	if (addr != MCID_NODEP) {
		EPRecord *epaddr=getEPRecord(addr);
		addRecordDepTable(epaddr,currrecord, VC_ADDRINDEX);
	}
}

/** @brief Processes a store. */

void MCExecution::store(void *addr, uint64_t val, int len) {
#ifndef TSO
	switch(len) {
	case 1:
		(*(uint8_t *)addr) = (uint8_t)val;
		break;
	case 2:
		(*(uint16_t *)addr) = (uint16_t)val;
		break;
	case 4:
		(*(uint32_t *)addr) = (uint32_t)val;
		break;
	case 8:
		(*(uint64_t *)addr) = (uint64_t)val;
		break;
	}
#endif

	if (DBG_ENABLED()) {
		model_print("STORE *%p=%llu ", addr, val);
		currexecpoint->print();
		model_print("\n");
	}
	EPRecord * record=getOrCreateCurrRecord(STORE, NULL, id_addr_offset, 1, len, false);
	recordStoreChange(record, addr, val);
	addStoreTable(addr, record);
	EPValue * epvalue=getEPValue(record, NULL, addr, val, len);
#ifdef TSO
	uint tid=id_to_int(currexecpoint->get_tid());
	(*storebuffer)[tid]->push_back(epvalue);
#endif
	MCID mcids[]={id_value};
	recordContext(epvalue, MCID_NODEP, id_addr, 1, mcids);
	MCID stmcid=getNextMCID();
	ASSERT(stmcid==EPList->size());
	EPList->push_back(epvalue);
	memtable->put(addr, stmcid);
	currexecpoint->incrementTop();
}

void MCExecution::recordFunctionChange(EPRecord *function, uint64_t val) {
	if (function->getSet(VC_FUNCOUTINDEX)->add(val))
		planner->addChange(new MCChange(function, val, VC_FUNCOUTINDEX));
}

void MCExecution::recordRMWChange(EPRecord *rmw, const void *addr, uint64_t valarg, uint64_t oldval, uint64_t newval) {
	if (!rmw->hasAddr(addr)) {
		rmw->getSet(VC_ADDRINDEX)->add((uint64_t)addr);
		planner->addChange(new MCChange(rmw, (uint64_t)addr, VC_ADDRINDEX));
	}
	rmw->getSet(VC_BASEINDEX)->add(valarg);
	if (rmw->getOp()==CAS)
		rmw->getSet(VC_OLDVALCASINDEX)->add(oldval);
	if (!rmw->getSet(VC_RMWOUTINDEX)->contains(newval)) {
		rmw->getSet(VC_RMWOUTINDEX)->add(newval);
		planner->addChange(new MCChange(rmw, newval, VC_RMWOUTINDEX));
	}
}

void MCExecution::recordLoadChange(EPRecord *load, const void *addr) {
	if (!load->hasAddr(addr)) {
		load->getSet(VC_ADDRINDEX)->add((uint64_t)addr);
		planner->addChange(new MCChange(load, (uint64_t)addr, VC_ADDRINDEX));
	}
}

void MCExecution::recordStoreChange(EPRecord *store, const void *addr, uint64_t val) {
	if (!store->hasAddr(addr)) {
		store->getSet(VC_ADDRINDEX)->add((uint64_t)addr);
		planner->addChange(new MCChange(store, (uint64_t)addr, VC_ADDRINDEX));
	}
	if (!store->hasValue(val)) {
		store->getSet(VC_BASEINDEX)->add(val);
		planner->addChange(new MCChange(store, val, VC_BASEINDEX));
	}
}

/** @brief Processes a load. */

uint64_t MCExecution::load(const void *addr, int len) {
#ifdef TSO
	uint64_t val=0;
	bool found=false;
	uint tid=id_to_int(currexecpoint->get_tid());
	SnapList<EPValue *> *list=(*storebuffer)[tid];
	for(SnapList<EPValue *>::reverse_iterator it=list->rbegin();it != list->rend();it++) {
		EPValue *epval=*it;
		const void *epaddr=epval->getAddr();
		if (epaddr == addr) {
			val = epval->getValue();
			found = true;
			break;
		}
	}
	if (!found) {
		switch(len) {
		case 1:
			val=*(uint8_t *) addr;
			break;
		case 2:
			val=*(uint16_t *) addr;
			break;
		case 4:
			val=*(uint32_t *) addr;
			break;
		case 8:
			val=*(uint64_t *) addr;
			break;
		}
	}
#else
	uint64_t val=0;
	switch(len) {
	case 1:
		val=*(uint8_t *) addr;
		break;
	case 2:
		val=*(uint16_t *) addr;
		break;
	case 4:
		val=*(uint32_t *) addr;
		break;
	case 8:
		val=*(uint64_t *) addr;
		break;
	}
#endif

	if (DBG_ENABLED()) {
		model_print("%llu(mid=%u)=LOAD %p ", val, id_retval, addr);
		currexecpoint->print();
		model_print("\n");
	}
	ASSERT(id_addr==MCID_NODEP || (getEPValue(id_addr)->getValue()+id_addr_offset)==((uint64_t)addr));
	EPRecord * record=getOrCreateCurrRecord(LOAD, NULL, id_addr_offset, 0, len, false);
	recordLoadChange(record, addr);
	addLoadTable(addr, record);
	EPValue * epvalue=getEPValue(record, NULL, addr, val, len);
	MCID vcrf=memtable->get(addr);
	recordContext(epvalue, vcrf, id_addr, 0, NULL);
	ASSERT(EPList->size()==id_retval);
	EPList->push_back(epvalue);
	currexecpoint->incrementTop();
	return val;
}

/** @brief Processes a branch. */

MCID MCExecution::branchDir(MCID dirid, int direction, int numdirs, bool anyvalue) {
	if (DBG_ENABLED()) {
		model_print("BRANCH dir=%u mid=%u", direction, dirid);
		currexecpoint->print();
		model_print("\n");
	}
	EPRecord * record=getOrCreateCurrRecord(BRANCHDIR, NULL, 0, 1, numdirs, anyvalue);
	bool isNew;
	EPValue * epvalue=getEPValue(record, &isNew, NULL, direction, 32);
	if (isNew) {
#ifdef PRINT_ACHIEVED_GOALS
		model_print("Achieved Goal: BRANCH dir=%u mid=%u", direction, dirid);
		currexecpoint->print();model_print("\n");
#endif
#ifdef STATS
		planner->getConstGen()->curr_stat->bgoals++;
#endif
		planner->getConstGen()->achievedGoal(record);
		scheduler->setNewFlag();
	}
	//record that we took a branch
	currbranch=epvalue;
	currbranch->lastrecord=NULL;
	MCID mcids[]={dirid};
	recordContext(epvalue, MCID_NODEP, MCID_NODEP, 1, mcids);
	MCID brmcid=getNextMCID();
	ASSERT(EPList->size()==brmcid);
	EPList->push_back(epvalue);
	//push the direction of the branch
	currexecpoint->push(EP_BRANCH,direction);
	//push the statement counter for the branch
	currexecpoint->push(EP_COUNTER,0);

	return brmcid;
}

/** @brief Processes a merge with a previous branch.  mcid gives the
                branch we merged. */

void MCExecution::merge(MCID mcid) {
	EPValue * epvalue=getEPValue(mcid);
	ExecPoint *ep=epvalue->getEP();
	//rollback current branch
	currbranch=EPTable->get(ep)->getBranch();
	int orig_length=ep->getSize();
	int curr_length=currexecpoint->getSize();
	for(int i=0;i<curr_length-orig_length;i++)
		currexecpoint->pop();
	//increment top
	currexecpoint->incrementTop();
	//now we can create the merge point
	if (DBG_ENABLED()) {
		model_print("MERGE mid=%u", mcid);
		currexecpoint->print();
		model_print("\n");
	}
	getOrCreateCurrRecord(MERGE, NULL, 0, 0, 8, false);
	currexecpoint->incrementTop();
}

/** @brief Phi function. */
MCID MCExecution::phi(MCID input) {
	EPRecord * record=getOrCreateCurrRecord(FUNCTION, NULL, 0, 1, 8, false);
	record->setPhi();
	EPValue *epinput=getEPValue(input);
	EPValue * epvalue=getEPValue(record, NULL, NULL, epinput->getValue(), 8);

	MCID mcids[]={input};
	recordContext(epvalue, MCID_NODEP, MCID_NODEP, 1, mcids);

	MCID fnmcid=getNextMCID();
	ASSERT(EPList->size()==fnmcid);
	EPList->push_back(epvalue);
	currexecpoint->incrementTop();

	return fnmcid;
}

/** @brief Phi function for loops. */
MCID MCExecution::loop_phi(MCID input) {
	EPRecord * record=getOrCreateCurrRecord(FUNCTION, NULL, 0, 1, 8, false);
	record->setLoopPhi();
	EPValue *epinput=getEPValue(input);
	EPValue * epvalue=getEPValue(record, NULL, NULL, epinput->getValue(), 8);

	MCID mcids[]={input};
	recordContext(epvalue, MCID_NODEP, MCID_NODEP, 1, mcids);

	uint tid=id_to_int(currexecpoint->get_tid());
	EPRecord *exitrec=(*lastloopexit)[tid];

	EPRecordIDSet *phiset=record->getPhiLoopTable();
	struct RecordIDPair rip={exitrec,getEPRecord(input)};
	if (!phiset->contains(&rip)) {
		struct RecordIDPair *p=(struct RecordIDPair *)model_malloc(sizeof(struct RecordIDPair));
		*p=rip;
		phiset->add(p);
	}

	MCID fnmcid=getNextMCID();
	ASSERT(EPList->size()==fnmcid);
	EPList->push_back(epvalue);
	currexecpoint->incrementTop();

	return fnmcid;
}

uint64_t MCExecution::equals(MCID op1, uint64_t val1, MCID op2, uint64_t val2, MCID *retval) {
	uint64_t eq=(val1==val2);
	bool isNew=false;
	int len=8;
	EPRecord * record=getOrCreateCurrRecord(EQUALS, &isNew, 0, 2, len, false);
	EPValue * epvalue=getEPValue(record, NULL, NULL, eq, len);
	getEPValue(record, NULL, NULL, 1-eq, len);
	MCID mcids[]={op1, op2};
	recordContext(epvalue, MCID_NODEP, MCID_NODEP, 2, mcids);
	if (isNew) {
		recordFunctionChange(record, 0);
		recordFunctionChange(record, 1);
	}
	MCID eqmcid=getNextMCID();
	ASSERT(EPList->size()==eqmcid);
	EPList->push_back(epvalue);
	currexecpoint->incrementTop();
	*retval=eqmcid;
	return eq;
}

/** @brief Processes an uninterpretted function. */

MCID MCExecution::function(uint functionidentifier, int len, uint64_t val, uint numids, MCID *mcids) {
	bool isNew=false;
	EPRecord * record=getOrCreateCurrRecord(FUNCTION, &isNew, 0, numids, len, false);
	if (isNew) {
		if (functionidentifier == 0)
			record->setCompletedGoal(new CGoalSet(), false);
		else {
			if (sharedgoals->size()<=functionidentifier) {
				sharedgoals->resize(functionidentifier+1);
				sharedfuncrecords->resize(functionidentifier+1);
			}
			if ((*sharedgoals)[functionidentifier]==NULL) {
				(*sharedgoals)[functionidentifier]=new CGoalSet();
				(*sharedfuncrecords)[functionidentifier]=new RecordSet();
			}
			record->setCompletedGoal((*sharedgoals)[functionidentifier], true);
			(*sharedfuncrecords)[functionidentifier]->add(record);

			for(uint i=VC_BASEINDEX;i<(numids+VC_BASEINDEX);i++) {
				EPRecord *input=getEPRecord(mcids[i-VC_BASEINDEX]);
				IntIterator *inputit=input->getReturnValueSet()->iterator();
				IntHashSet *recinputset=record->getSet(i);
				while(inputit->hasNext()) {
					uint64_t inputval=inputit->next();
					recinputset->add(inputval);
				}
				delete inputit;
			}

			CGoalIterator *cit=(*sharedgoals)[functionidentifier]->iterator();
			while(cit->hasNext()) {
				CGoal *goal=cit->next();
				evalGoal(record, goal);
			}
			delete cit;
		}
	}
	val=val&getmask(len);
	EPValue * epvalue=getEPValue(record, NULL, NULL, val, len);
	recordContext(epvalue, MCID_NODEP, MCID_NODEP, numids, mcids);

	uint64_t valarray[numids+VC_BASEINDEX];
	for(uint i=0;i<VC_BASEINDEX;i++) {
		valarray[i]=0;
	}
	for(uint i=VC_BASEINDEX;i<(numids+VC_BASEINDEX);i++) {
		valarray[i]=getEPValue(mcids[i-VC_BASEINDEX])->getValue();
	}

	MCID fnmcid=getNextMCID();
	ASSERT(EPList->size()==fnmcid);
	EPList->push_back(epvalue);
	currexecpoint->incrementTop();

	CGoal *goal=new CGoal(numids+VC_BASEINDEX, valarray);
	if (record->completedGoalSet()->contains(goal)) {
		delete goal;
	} else {
		scheduler->setNewFlag();
		planner->getConstGen()->achievedGoal(record);
#ifdef PRINT_ACHIEVED_GOALS
		model_print("Achieved goal:");goal->print();model_print("\n");
#endif
#ifdef STATS
		planner->getConstGen()->curr_stat->fgoals++;
#endif
		goal->setOutput(val);
		record->completedGoalSet()->add(goal);
		recordFunctionChange(record, val);
		if (functionidentifier != 0) {
			RecordIterator *rit=(*sharedfuncrecords)[functionidentifier]->iterator();
			while(rit->hasNext()) {
				EPRecord *func=rit->next();
				if (func == record)
					continue;
				evalGoal(func, goal);
			}
			delete rit;
		}
	}

	return fnmcid;
}

void MCExecution::evalGoal(EPRecord *record, CGoal *goal) {
	for(uint i=VC_BASEINDEX;i<goal->getNum();i++) {
		uint64_t input=goal->getValue(i);
		if (!record->getSet(i)->contains(input))
			return;
	}
	//Have all input, propagate output
	recordFunctionChange(record, goal->getOutput());
}

/** @brief Returns the next thread id. */

thread_id_t MCExecution::get_next_tid() {
	return int_to_id(snap_fields->next_thread_id++);
}

/** @brief Registers a new thread */

void MCExecution::add_thread(Thread *t) {
	ThreadList->push_back(t);
	ExecPoint * ep=new ExecPoint(4, t->get_id());
	ep->push(EP_COUNTER,0);
	ExecPointList->push_back(ep);
	CurrBranchList->push_back(NULL);
#ifdef TSO
	storebuffer->push_back(new SnapList<EPValue *>());
#endif
}

/** @brief Records currently executing thread. */

void MCExecution::set_current_thread(Thread *t) {
	if (curr_thread) {
		uint oldtid=id_to_int(curr_thread->get_id());
		(*CurrBranchList)[oldtid]=currbranch;
	}
	curr_thread=t;
	currexecpoint=(t==NULL) ? NULL : (*ExecPointList)[id_to_int(t->get_id())];
	currbranch=(t==NULL) ? NULL : (*CurrBranchList)[id_to_int(t->get_id())];
}

void MCExecution::threadStart(EPRecord *parent) {
	EPRecord *threadstart=getOrCreateCurrRecord(THREADBEGIN, NULL, 0, 0, 8, false);
	if (parent!=NULL) {
		parent->setChildRecord(threadstart);
	}
	currexecpoint->incrementTop();
}

/** @brief Create a new thread. */

void MCExecution::threadCreate(thrd_t *t, thrd_start_t start, void *arg) {
	EPRecord *threadstart=getOrCreateCurrRecord(THREADCREATE, NULL, 0, 0, 8, false);
	currexecpoint->incrementTop();
	Thread *th = new Thread(get_next_tid(), t, start, arg, curr_thread, threadstart);
	add_thread(th);
}

/** @brief Joins with a thread. */

void MCExecution::threadJoin(Thread *blocking) {
	while(true) {
		if (blocking->is_complete()) {
			EPRecord *join=getOrCreateCurrRecord(THREADJOIN, NULL, 0, 0, 8, false);
			currexecpoint->incrementTop();
			join->setJoinThread(blocking->get_id());
			return;
		}
		get_current_thread()->set_waiting(blocking);
		scheduler->check_preempt();
		get_current_thread()->set_waiting(NULL);
	}
}

/** @brief Finishes a thread. */

void MCExecution::threadFinish() {
	Thread *th = get_current_thread();
	/* Wake up any joining threads */
	for (unsigned int i = 0;i < get_num_threads();i++) {
		Thread *waiting = get_thread(int_to_id(i));
		if (waiting->waiting_on() == th) {
			waiting->set_waiting(NULL);
		}
	}
	th->complete();
	scheduler->check_preempt();
}

/** @brief Thread yield. */

void MCExecution::threadYield() {
	getOrCreateCurrRecord(YIELD, NULL, 0, 0, 8, false);
	currexecpoint->incrementTop();
	if (model->params.noexecyields) {
		threadFinish();
	}
}

/** @brief Thread yield. */

void * MCExecution::alloc(size_t size) {
	bool isNew;
	EPRecord *record=getOrCreateCurrRecord(ALLOC, &isNew, 0, 0, 8, false);
	currexecpoint->incrementTop();
	if (isNew) {
		size_t allocsize=((size<<1)+7)&~((size_t)(7));
		record->setSize(allocsize);
		void *ptr=real_user_malloc(size);
		record->setPtr(ptr);
		return ptr;
	} else {
		if (size>record->getSize()) {
			model_print("Allocation size changed too much\n");
			exit(-1);
		}
		void *ptr=record->getPtr();
		return ptr;
	}
}

/** @brief Record enter loop. */

void MCExecution::enterLoop() {
	EPRecord * record=getOrCreateCurrRecord(LOOPENTER, NULL, 0, 0, 8, false);

	//push the loop iteration counter
	currexecpoint->push(EP_LOOP,0);
	//push the curr iteration statement counter
	currexecpoint->push(EP_COUNTER,0);
	EPRecord * lpstartrecord=getOrCreateCurrRecord(LOOPSTART, NULL, 0, 0, 8, false);
	record->setChildRecord(lpstartrecord);

	currexecpoint->incrementTop();
	if (DBG_ENABLED()) {
		model_print("ENLOOP ");
		currexecpoint->print();
		model_print("\n");
	}
}

/** @brief Record exit loop. */

void MCExecution::exitLoop() {
	ExecPointType type;
	EPRecord *breakstate=NULL;

	/* Record last statement */
	uint tid=id_to_int(currexecpoint->get_tid());

	if (!currexecpoint->directInLoop()) {
		breakstate=getOrCreateCurrRecord(NONLOCALTRANS, NULL, 0, 0, 8, false);
		currexecpoint->incrementTop();
	} else {
		breakstate=getOrCreateCurrRecord(LOOPEXIT, NULL, 0, 0, 8, false);
		currexecpoint->incrementTop();
	}

	/* Get Last Record */
	EPRecord *lastrecord=(currbranch==NULL) ? (*alwaysexecuted)[tid] : currbranch->lastrecord;

	/* Remember last record as loop exit for this execution.  */
	if (lastloopexit->size()<=tid) {
		lastloopexit->resize(tid+1);
	}
	(*lastloopexit)[tid]=lastrecord;

	do {
		type=currexecpoint->getType();
		currexecpoint->pop();
		if (type==EP_BRANCH) {
			//we crossed a branch, fix currbranch
			EPRecord *branch=currbranch->getRecord();
			currbranch=branch->getBranch();
		}
	} while(type!=EP_LOOP);

	if (DBG_ENABLED()) {
		model_print("EXLOOP ");
		currexecpoint->print();
		model_print("\n");
	}
	EPRecord *loopenter=EPTable->get(currexecpoint);
	currexecpoint->incrementTop();
	EPRecord *labelrec=getOrCreateCurrRecord(LABEL, NULL, 0, 0, 8, false);
	if (loopenter->getNextRecord()==NULL) {
		loopenter->setNextRecord(labelrec);
	}
	breakstate->setNextRecord(labelrec);
	currexecpoint->incrementTop();
}

/** @brief Record next loop iteration. */
void MCExecution::loopIterate() {
	currexecpoint->pop();
	//increment the iteration counter
	currexecpoint->incrementTop();
	currexecpoint->push(EP_COUNTER,0);
}

/** @brief Helper function to add new item to a StoreLoadHashTable */

void addTable(StoreLoadHashTable *table, const void *addr, EPRecord *record) {
	RecordSet * rset=table->get(addr);
	if (rset==NULL) {
		rset=new RecordSet();
		table->put(addr, rset);
	}
	rset->add(record);
}

/** @brief Update mapping from address to stores to that address. */

void MCExecution::addStoreTable(const void *addr, EPRecord *record) {
	addTable(storetable, addr, record);
}

/** @brief Update mapping from address to loads from that address. */

void MCExecution::addLoadTable(const void *addr, EPRecord *record) {
	addTable(loadtable, addr, record);
}

/** @brief Update mapping from address to loads from that address. */

RecordSet * MCExecution::getLoadTable(const void *addr) {
	return loadtable->get(addr);
}

/** @brief Update mapping from address to stores to that address. */

RecordSet * MCExecution::getStoreTable(const void *addr) {
	return storetable->get(addr);
}

/** @brief Registers that component index of deprecord depends on record. */

void MCExecution::addRecordDepTable(EPRecord *record, EPRecord *deprecord, unsigned int index) {
	EPRecordSet *set=eprecorddep->get(record);
	if (set==NULL) {
		set=new EPRecordSet();
		eprecorddep->put(record, set);
	}
	struct RecordIntPair pair={deprecord, index};

	if (!set->contains(&pair)) {
		struct RecordIntPair *p=(struct RecordIntPair *)model_malloc(sizeof(struct RecordIntPair));
		*p=pair;
		set->add(p);
	}

	if (!revrecorddep->contains(&pair)) {
		struct RecordIntPair *p=(struct RecordIntPair *)model_malloc(sizeof(struct RecordIntPair));
		*p=pair;
		revrecorddep->put(p, new ModelVector<EPRecord *>());
	}

	ModelVector<EPRecord *> * recvec=revrecorddep->get(&pair);
	for(uint i=0;i<recvec->size();i++) {
		if ((*recvec)[i]==record)
			return;
	}
	recvec->push_back(record);
}

ModelVector<EPRecord *> * MCExecution::getRevDependences(EPRecord *record, uint index) {
	struct RecordIntPair pair={record, index};
	return revrecorddep->get(&pair);
}

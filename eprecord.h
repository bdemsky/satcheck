/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef EPRECORD_H
#define EPRECORD_H
#include "epvalue.h"
#include "stl-model.h"
#include "cgoal.h"
#include "hashset.h"
#include "modeltypes.h"
#include <libinterface.h>
#include <stdio.h>

typedef ModelVector<EPValue *> ValueVector;
const char * eventToStr(EventType event);
struct RecordIDPair {
	EPRecord *record;
	EPRecord *idrecord;
};

inline unsigned int RIDP_hash_function(struct RecordIDPair * pair) {
	return (unsigned int)(((uintptr_t)pair->record) >> 4) ^ ((uintptr_t)pair->idrecord);
}

inline bool RIDP_equals(struct RecordIDPair * key1, struct RecordIDPair * key2) {
	if (key1==NULL)
		return key1==key2;
	return key1->record == key2->record && key1->idrecord == key2->idrecord;
}

typedef HashSet<struct RecordIDPair *, uintptr_t, 0, model_malloc, model_calloc, model_free, RIDP_hash_function, RIDP_equals> EPRecordIDSet;

typedef HSIterator<struct RecordIDPair *, uintptr_t, 0, model_malloc, model_calloc, model_free, RIDP_hash_function, RIDP_equals> EPRecordIDIterator;

class EPRecord {
public:
	EPRecord(EventType event, ExecPoint *execpoint, EPValue *branch, uintptr_t _offset, unsigned int numinputs,uint len,  bool anyvalue = false);
	~EPRecord();
	ExecPoint * getEP() {return execpoint;}
	void addEPValue(EPValue *v) {valuevector->push_back(v);}
	bool hasAddr(const void *addr);
	bool hasValue(uint64_t val);
	unsigned int numValues() {return valuevector->size();}
	EPValue * getValue(unsigned int i) {return (*valuevector)[i];}
	EPValue * getBranch() {return branch;}
	int getIndex(EPValue *v);
	int getIndex(uint64_t val);
	EventType getType() {return event;}
	CGoalSet *completedGoalSet() {return completed;}
	IntHashSet * getSet(unsigned int i) {return setarray[i];}
	IntHashSet * getReturnValueSet();
	IntHashSet * getStoreSet() { if (event==RMW) return setarray[VC_RMWOUTINDEX]; else return setarray[VC_BASEINDEX];}
	uint getNumInputs() {return numinputs;}
	uint getNumFuncInputs() {return numinputs-VC_BASEINDEX;}
	bool getBranchAnyValue() {return anyvalue;}
	void print();
	void print(int fd);
	EPRecord *getNextRecord() {return nextRecord;}
	EPRecord *getChildRecord() {return childRecord;}
	void setNextRecord(EPRecord * n) {nextRecord=n;}
	void setChildRecord(EPRecord *n) {childRecord=n;}
	bool getPhi() {return phi;}
	void setPhi() {phi=true;}
	bool getLoopPhi() {return loopphi;}
	void setLoopPhi() {if (philooptable==NULL) philooptable=new EPRecordIDSet(); phi=true; loopphi=true;}
	void setRMW(enum atomicop _op) {op=_op;}
	enum atomicop getOp() {return op;}
	uint getLen() {return len;}
	void setJoinThread(thread_id_t _j) {jointhread=_j;}
	thread_id_t getJoinThread() {return jointhread;}
	EPRecordIDSet * getPhiLoopTable() {return philooptable;}
	void setSize(size_t s) {size=s;}
	size_t getSize() {return size;}
	void setPtr(void *p) {ptr=p;}
	void * getPtr() {return ptr;}
	void setCompletedGoal(CGoalSet *c, bool shared) {completed=c;func_shares_goalset=shared;}
	bool isSharedGoals() {return func_shares_goalset;}
	uintptr_t getOffset() {return offset;}
	MEMALLOC;

private:
	ValueVector * valuevector;
	ExecPoint * execpoint;
	CGoalSet *completed;
	EPValue *branch;
	IntHashSet **setarray;
	EPRecord *nextRecord;
	EPRecord *childRecord;
	EPRecordIDSet * philooptable;
	uintptr_t offset;
	EventType event;
	thread_id_t jointhread;
	unsigned int numinputs;
	uint len;
	bool anyvalue;
	bool phi;
	bool loopphi;
	enum atomicop op;
	size_t size;
	bool func_shares_goalset;
	void *ptr;
};

#endif

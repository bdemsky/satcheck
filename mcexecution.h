/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef MCEXECUTION_H
#define MCEXECUTION_H
#include "hashtable.h"
#include "execpoint.h"
#include "stl-model.h"
#include "classlist.h"
#include <libinterface.h>
#include "eprecord.h"
#include "epvalue.h"
#include "threads-model.h"

typedef HashTable<ExecPoint *, EPRecord *, uintptr_t, 0, model_malloc, model_calloc, model_free, ExecPointHash, ExecPointEquals> EPHashTable;
typedef HashTable<const void *, MCID, uintptr_t, 4, snapshot_malloc, snapshot_calloc, snapshot_free> MemHashTable;
typedef HashTable<EPValue *, EPValue *, uintptr_t, 0, model_malloc, model_calloc, model_free, EPValueHash, EPValueEquals> EPValueHashTable;
typedef HashTable<const void *, RecordSet *, uintptr_t, 4, model_malloc, model_calloc, model_free> StoreLoadHashTable;


struct RecordIntPair {
	EPRecord *record;
	unsigned int index;
};
inline unsigned int RP_hash_function(struct RecordIntPair * pair) {
	return (unsigned int)(((uintptr_t)pair->record) >> 4) ^ pair->index;
}
inline bool RP_equals(struct RecordIntPair * key1, struct RecordIntPair * key2) {
	if (key1==NULL)
		return key1==key2;
	return key1->record == key2->record && key1->index == key2->index;
}
typedef HashSet<struct RecordIntPair *, uintptr_t, 0, model_malloc, model_calloc, model_free, RP_hash_function, RP_equals> EPRecordSet;
typedef HSIterator<struct RecordIntPair *, uintptr_t, 0, model_malloc, model_calloc, model_free, RP_hash_function, RP_equals> EPRecordIterator;
typedef HashTable<EPRecord *, EPRecordSet *, uintptr_t, 4, model_malloc, model_calloc, model_free> EPRecordDepHashTable;
typedef HashTable<struct RecordIntPair *, ModelVector<EPRecord *> *, uintptr_t, 4, model_malloc, model_calloc, model_free, RP_hash_function, RP_equals> RevDepHashTable;

struct MCExecution_snapshot {
	MCExecution_snapshot();
	~MCExecution_snapshot();
	unsigned int next_thread_id;

	SNAPSHOTALLOC;
};

class MCExecution {
public:
	MCExecution();
	~MCExecution();
	EPRecord * getOrCreateCurrRecord(EventType et, bool * isNew, uintptr_t offset, unsigned int numinputs, uint len, bool br_anyvalue);
	MCID getNextMCID() { return ++currid; }
	MCID loadMCID(MCID maddr, uintptr_t offset);
	void storeMCID(MCID maddr, uintptr_t offset, MCID val);
	MCID nextRMW(MCID addr, uintptr_t offset, MCID oldval, MCID valarg);
	void store(void *addr, uint64_t val, int len);
	uint64_t load(const void *addr, int len);
	uint64_t rmw(enum atomicop op, void *addr, uint size, uint64_t currval, uint64_t oldval, uint64_t valarg);
	void reset();
	MCID branchDir(MCID dir, int direction, int numdirs, bool anyvalue);
	void merge(MCID mcid);
	MCID function(uint functionidentifier, int numbytesretval, uint64_t val, uint num_args, MCID *mcids);
	uint64_t equals(MCID op1, uint64_t val1, MCID op2, uint64_t val2, MCID *retval);
	MCID phi(MCID input);
	MCID loop_phi(MCID input);
	void * alloc(size_t bytes);
#ifdef TSO
	void fence();
	void doStore(EPValue *epval);
	void doStore(thread_id_t tid);
	bool isEmptyStoreBuffer(thread_id_t tid);
	EPValue *getStoreBuffer(thread_id_t tid) {int thread=id_to_int(tid); if ((*storebuffer)[thread]->empty()) return NULL; else return (*storebuffer)[id_to_int(tid)]->front();}
#endif
	void enterLoop();
	void exitLoop();
	void loopIterate();
	void threadCreate(thrd_t *t, thrd_start_t start, void *arg);
	void threadJoin(Thread *th);
	void threadYield();
	void threadFinish();
	MCScheduler * get_scheduler() const { return scheduler; }
	Planner * get_planner() const { return planner; }
	EPValue * getEPValue(EPRecord * record, bool * isNew, const void *addr, uint64_t val, int len);
	void recordContext(EPValue * epv, MCID rf, MCID addr, int numids, MCID *mcids);
	void add_thread(Thread *t);
	thread_id_t get_next_tid();
	unsigned int get_num_threads() {return snap_fields->next_thread_id;};
	void set_current_thread(Thread *t);
	Thread * get_current_thread() {return curr_thread;}
	ExecPoint * get_execpoint(thread_id_t tid) {return (*ExecPointList)[id_to_int(tid)];}
	Thread * get_thread(thread_id_t tid) {return (*ThreadList)[id_to_int(tid)];}
	void addStoreTable(const void *addr, EPRecord *record);
	void addLoadTable(const void *addr, EPRecord *record);
	RecordSet * getStoreTable(const void *addr);
	RecordSet * getLoadTable(const void *addr);
	void recordRMWChange(EPRecord *load, const void *addr, uint64_t valarg, uint64_t oldval, uint64_t newval);
	void recordFunctionChange(EPRecord *function, uint64_t val);
	void recordLoadChange(EPRecord *function, const void *addr);
	void recordStoreChange(EPRecord *function, const void *addr, uint64_t val);
	EPRecord * getEPRecord(MCID id);
	EPValue * getEPValue(MCID id);
	void addRecordDepTable(EPRecord *record, EPRecord *deprecord, unsigned int index);
	EPRecordSet *getDependences(EPRecord *record) {return eprecorddep->get(record);}
	ModelVector<EPRecord *> * getRevDependences(EPRecord *record, uint index);
	void threadStart(EPRecord *parent);

	bool compatibleStoreLoad(EPRecord *store, EPRecord *load) {
		return store!=load;
	}
	ModelVector<EPRecord *> * getAlwaysExecuted() {return alwaysexecuted;}
	ModelVector<EPRecord *> * getJoins() {return joinvec;}
	void evalGoal(EPRecord *record, CGoal *goal);
	void dumpExecution();
	EPRecord * getRecord(ExecPoint * ep) {return EPTable->get(ep);}

	MEMALLOC;
private:
	EPHashTable * EPTable;
	EPValueHashTable * EPValueTable;
	MemHashTable *memtable;
	StoreLoadHashTable *storetable;
	StoreLoadHashTable *loadtable;
	SnapVector<EPValue *> *EPList;
	SnapVector<Thread *> *ThreadList;
#ifdef TSO
	SnapVector<SnapList<EPValue *> *> *storebuffer;
#endif
	SnapVector<ExecPoint *> *ExecPointList;
	SnapVector<EPValue *> *CurrBranchList;
	ExecPoint * currexecpoint;
	MCScheduler * scheduler;
	Planner * planner;
	struct MCExecution_snapshot * const snap_fields;
	Thread * curr_thread;
	EPValue *currbranch;
	EPRecordDepHashTable *eprecorddep;
	RevDepHashTable *revrecorddep;
	ModelVector<EPRecord *> * alwaysexecuted;
	ModelVector<EPRecord *> * lastloopexit;
	ModelVector<EPRecord *> * joinvec;
	ModelVector<CGoalSet *> * sharedgoals;
	ModelVector<RecordSet *> * sharedfuncrecords;

	MCID currid;
	MCID id_addr;
	MCID id_oldvalue;
	MCID id_value;
	MCID id_retval;
	uintptr_t id_addr_offset;
	uint schedule_graph;
};

#endif

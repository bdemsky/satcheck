/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef CONSTGEN_H
#define CONSTGEN_H
#include "classlist.h"
#include "stl-model.h"


#ifdef VERBOSE_CONSTRAINTS
#define ADDCONSTRAINT(cn, str) do {Constraint *c=cn; model_print("%s ",str);c->print();model_print("\n");addConstraint(c);} while(0);
#define ADDCONSTRAINT2(cg, cn, str) do {Constraint *c=cn; model_print("%s ",str);c->print();model_print("\n");cg->addConstraint(c);} while(0);

#define ADDGOAL(r, cn, str) do {Constraint *c=cn; model_print("%s ",str);c->print();model_print("\n");addGoal(r, c);} while(0);
#define ADDBRANCHGOAL(r, cn, str) do {Constraint *c=cn; model_print("%s ",str);c->print();model_print("\n");addBranchGoal(r, c);} while(0);
#else

#define ADDCONSTRAINT(cn, store) addConstraint(cn);
#define ADDCONSTRAINT2(cg, cn, store) cg->addConstraint(cn);

#define ADDGOAL(r, cn, store) addGoal(r, cn);
#define ADDBRANCHGOAL(r, cn, store) addBranchGoal(r, cn);
#endif

unsigned int RecPairHash(RecPair *rp);
bool RecPairEquals(RecPair *r1, RecPair *r2);
typedef HashTable<RecPair *, RecPair *, uintptr_t, 0, model_malloc, model_calloc, model_free, RecPairHash, RecPairEquals> RecPairTable;

typedef HashTable<EPRecord *, uint, uintptr_t, 0, model_malloc, model_calloc, model_free> RecToIntTable;

#ifdef STATS
struct MC_Stat {
	long long time;
	int bgoals;
	int fgoals;
	bool was_incremental;
	bool was_sat;
	MEMALLOC;
};
#endif

class ConstGen {
public:
	ConstGen(MCExecution *e);
	~ConstGen();
	bool process();
	void reset();
	bool canReuseEncoding();
	Constraint * getNewVar();
	void getArrayNewVars(uint num, Constraint **);
	RecordSet * computeConflictSet(EPRecord *store, EPRecord *load, RecordSet *baseset);
	RecordSet * getMayReadFromSet(EPRecord *load);
	RecordSet * getMayReadFromSetOpt(EPRecord *load);
	void addConstraint(Constraint *c);
	bool * runSolver();
	StoreLoadSet * getStoreLoadSet(EPRecord *op);
	Constraint * getOrderConstraint(EPRecord *first, EPRecord *second);
	Constraint * getExecutionConstraint(EPRecord *record);
	bool isAlwaysExecuted(EPRecord *record);
	BranchRecord * getBranchRecord(EPRecord *branch) {return branchtable->get(branch);}
	bool getOrder(EPRecord *first, EPRecord *second, bool * satsolution);
	void printConstraint(uint value);
	void printNegConstraint(uint value);
	void achievedGoal(EPRecord *record);
#ifdef STATS
	MC_Stat *curr_stat;
#endif

	MEMALLOC;
private:
	Constraint * getRetValueEncoding(EPRecord *record, uint64_t value);
	Constraint * getMemValueEncoding(EPRecord *record, uint64_t value);
	bool subsumesExecutionConstraint(EPRecord *recordsubsumes, EPRecord *record);

	void addBranchGoal(EPRecord *, Constraint *c);
	void addGoal(EPRecord *, Constraint *c);
	void translateGoals();

	/** Methods to print execution graph .*/
	void printEventGraph();
	void printRecord(EPRecord *record, int file);
	void doPrint(EPRecord *record, int file);

	/** Methods to traverse execution graph .*/
	void traverse(bool initial);
	void traverseRecord(EPRecord *record, bool initial);

	/** Methods for initially visit records */
	void visitRecord(EPRecord *record);
	void insertAction(EPRecord *record);
	void insertFunction(EPRecord *record);
	void insertEquals(EPRecord *record);
	void insertBranch(EPRecord *record);
	void insertNonLocal(EPRecord *record);
	void insertLabel(EPRecord *record);

	/** Methods for second pass over records */
	void processRecord(EPRecord *record);
	void processYield(EPRecord *record);
	void processFunction(EPRecord *record);
	void processEquals(EPRecord *record);
	void processLoopPhi(EPRecord *record);
	void processPhi(EPRecord *record);
	void processCAS(EPRecord *record);
	void processEXC(EPRecord *record);
	void processAdd(EPRecord *record);
	void processStore(EPRecord *record);
	void processBranch(EPRecord *record);
	void processLoad(EPRecord *record);
	void processAddresses(EPRecord *record);
	void recordExecCond(EPRecord *record);
	void computeYieldCond(EPRecord *record);
	
	/** TSO Specific methods */
#ifdef TSO
	void genTSOTransOrderConstraints();
	void insertTSOAction(EPRecord *record);
	void processFence(EPRecord *record);
#endif

	/** These functions build closed groups of memory operations that
	                can store/load from each other. */
	void groupMemoryOperations(EPRecord *op);
	void mergeSets(StoreLoadSet *to, StoreLoadSet *from);

	bool orderThread(EPRecord *first, EPRecord *second);
	void createOrderConstraint(EPRecord *r1, EPRecord *r2);
	void genTransOrderConstraints();
	void genTransOrderConstraints(ModelVector<EPRecord *> * t1vec, ModelVector<EPRecord *> *t2vec, ModelVector<EPRecord *> * t3vec);
	void genTransOrderConstraints(ModelVector<EPRecord *> * t1vec, ModelVector<EPRecord *> *t2vec);
	void genTransOrderConstraint(EPRecord *t1rec, EPRecord *t2rec, EPRecord *t3rec);

	/** The hashtable maps an address to the closure set of
	                memory operations that may touch that address. */
	StoreLoadSetHashTable *storeloadtable;
	/** This hashtable maps a load to all of the stores it may read
	                from. */
	LoadHashTable *loadtable;

	MCExecution *execution;
	ModelVector<EPRecord *> * workstack;
	ModelVector<ModelVector<EPRecord *> *> * threadactions;
	RecPairTable *rpt;
	uint numconstraints;
	ModelVector<Constraint *> * goalset;
	ModelVector<EPRecord *> *yieldlist;
	Constraint ** goalvararray;
	ModelVector<Constraint *> * vars;
	BranchTable * branchtable;
	FunctionTable *functiontable;
	EqualsTable *equalstable;
	ScheduleBuilder *schedulebuilder;
	RecordSet *nonlocaltrans;
	RecordSet *incompleteexploredbranch;
	LoadHashTable *execcondtable;
	IncrementalSolver *solver;
	RecToIntTable *rectoint;
	RecToConsTable *yieldtable;
#ifdef STATS
	ModelVector<struct MC_Stat *> * stats;
#endif
	uint varindex;
	int schedule_graph;
	bool has_untaken_branches;
};

class RecPair {
public:
	RecPair(EPRecord *r1, EPRecord *r2) :
		epr1(r1),
		epr2(r2),
		constraint(NULL) {
	}
	EPRecord *epr1;
	EPRecord *epr2;
	Constraint *constraint;
	MEMALLOC;
};

#endif

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "constgen.h"
#include "mcexecution.h"
#include "constraint.h"
#include "branchrecord.h"
#include "functionrecord.h"
#include "equalsrecord.h"
#include "storeloadset.h"
#include "loadrf.h"
#include "schedulebuilder.h"
#include "model.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "inc_solver.h"
#include <sys/time.h>

long long myclock() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec*1000000+tv.tv_usec;
}

ConstGen::ConstGen(MCExecution *e) :
	storeloadtable(new StoreLoadSetHashTable()),
	loadtable(new LoadHashTable()),
	execution(e),
	workstack(new ModelVector<EPRecord *>()),
	threadactions(new ModelVector<ModelVector<EPRecord *> *>()),
	rpt(new RecPairTable()),
	numconstraints(0),
	goalset(new ModelVector<Constraint *>()),
	yieldlist(new ModelVector<EPRecord *>()),
	goalvararray(NULL),
	vars(new ModelVector<Constraint *>()),
	branchtable(new BranchTable()),
	functiontable(new FunctionTable()),
	equalstable(new EqualsTable()),
	schedulebuilder(new ScheduleBuilder(e, this)),
	nonlocaltrans(new RecordSet()),
	incompleteexploredbranch(new RecordSet()),
	execcondtable(new LoadHashTable()),
	solver(NULL),
	rectoint(new RecToIntTable()),
	yieldtable(new RecToConsTable()),
	varindex(1),
	schedule_graph(0),
	has_untaken_branches(false)
{
#ifdef STATS
	curr_stat=new MC_Stat();
	curr_stat->time=0;
	curr_stat->was_incremental=false;
	curr_stat->was_sat=true;
	curr_stat->fgoals=0;
	curr_stat->bgoals=0;
	stats=new ModelVector<struct MC_Stat *>();
#endif
}

ConstGen::~ConstGen() {
	reset();
	if (solver != NULL)
		delete solver;
	for(uint i=0;i<vars->size();i++) {
		Constraint *var=(*vars)[i];
		Constraint *notvar=var->neg;
		delete var;
		delete notvar;
	}
	vars->clear();
	delete storeloadtable;
	delete loadtable;
	delete workstack;
	delete threadactions;
	delete rpt;
	delete goalset;
	delete yieldlist;
	delete vars;
	delete branchtable;
	delete functiontable;
	delete equalstable;
	delete schedulebuilder;
	delete nonlocaltrans;
	delete incompleteexploredbranch;
	delete execcondtable;
	delete rectoint;
	delete yieldtable;
}

void resetstoreloadtable(StoreLoadSetHashTable * storeloadtable) {
	for(uint i=0;i<storeloadtable->capacity;i++) {
		struct hashlistnode<const void *, StoreLoadSet *> * ptr=&storeloadtable->table[i];
		if (ptr->val != NULL) {
			if (ptr->val->removeAddress(ptr->key))
				delete ptr->val;
			ptr->val=NULL;
			ptr->key=NULL;
		}
	}

	if (storeloadtable->zero != NULL) {
		if (storeloadtable->zero->val->removeAddress(storeloadtable->zero->key)) {
			delete storeloadtable->zero->val;
		} else
			ASSERT(false);
		model_free(storeloadtable->zero);
		storeloadtable->zero = NULL;
	}
	storeloadtable->size = 0;
}

void ConstGen::reset() {
	for(uint i=0;i<threadactions->size();i++) {
		delete (*threadactions)[i];
	}
	if (goalvararray != NULL) {
		model_free(goalvararray);
		goalvararray=NULL;
	}
	functiontable->resetanddelete();
	equalstable->resetanddelete();
	branchtable->resetanddelete();
	loadtable->resetanddelete();
	execcondtable->resetanddelete();
	rpt->resetanddelete();
	resetstoreloadtable(storeloadtable);
	nonlocaltrans->reset();
	incompleteexploredbranch->reset();
	threadactions->clear();
	rectoint->reset();
	yieldtable->reset();
	yieldlist->clear();
	goalset->clear();
	varindex=1;
	numconstraints=0;
	has_untaken_branches=false;
}

void ConstGen::translateGoals() {
	int size=goalset->size();
	goalvararray=(Constraint **) model_malloc(size*sizeof(Constraint *));

	for(int i=0;i<size;i++) {
		Constraint *goal=(*goalset)[i];
		goalvararray[i]=getNewVar();
		addConstraint(new Constraint(OR, goalvararray[i]->negate(), goal));
	}
	Constraint *atleastonegoal=new Constraint(OR, size, goalvararray);
	ADDCONSTRAINT(atleastonegoal, "atleastonegoal");
}

bool ConstGen::canReuseEncoding() {
	if (solver == NULL)
		return false;
	Constraint *array[goalset->size()];
	int remaininggoals=0;

	//Zero out the achieved goals
	for(uint i=0;i<goalset->size();i++) {
		Constraint *var=goalvararray[i];
		if (var != NULL) {
			array[remaininggoals++]=var;
		}
	}
	if (remaininggoals == 0)
		return false;
	Constraint *atleastonegoal=new Constraint(OR, remaininggoals, array);
	ADDCONSTRAINT(atleastonegoal, "atleastonegoal");
	solver->finishedClauses();
	clock_t start=myclock();
	bool *solution=runSolver();
	clock_t finish=myclock();
#ifdef STATS
	if (curr_stat != NULL)
		stats->push_back(curr_stat);
	curr_stat=new MC_Stat();
	curr_stat->time=finish-start;
	curr_stat->was_incremental=true;
	curr_stat->was_sat=(solution!=NULL);
	curr_stat->fgoals=0;
	curr_stat->bgoals=0;
#endif
	model_print("start %lu, finish %lu\n", start,finish);
	model_print("Incremental time = %12.6f\n", ((double)(finish-start))/1000000);

	if (solution==NULL) {
		return false;
	}

	//Zero out the achieved goals
	for(uint i=0;i<goalset->size();i++) {
		Constraint *var=goalvararray[i];
		if (var != NULL) {
			if (solution[var->getVar()]) {
				//if (goalvararray[i] != NULL)
				//model_print("SAT clearing goal %d.\n", i);

				goalvararray[i]=NULL;
			}
		}
	}

	schedulebuilder->buildSchedule(solution);
	model_free(solution);

	return true;
}

bool ConstGen::process() {
#ifdef DUMP_EVENT_GRAPHS
	printEventGraph();
#endif

	if (solver != NULL) {
		delete solver;
		solver = NULL;
	}

	solver=new IncrementalSolver();

	traverse(true);
	genTransOrderConstraints();
#ifdef TSO
	genTSOTransOrderConstraints();
#endif
	traverse(false);
	translateGoals();
	if (goalset->size()==0) {
#ifdef STATS
		stats->push_back(curr_stat);
		for(uint i=0;i<stats->size();i++) {
			struct MC_Stat *s=(*stats)[i];
			model_print("time=%lld fgoals=%d bgoals=%d incremental=%d sat=%d\n", s->time, s->fgoals, s->bgoals, s->was_incremental, s->was_sat);
		}
#endif
		model_print("No goals, done\n");
		delete solver;
		solver = NULL;
		return true;
	}

	if (model->params.branches && !has_untaken_branches) {
		delete solver;
		solver = NULL;
		return true;
	}

	solver->finishedClauses();

	//Freeze the goal variables
	for(uint i=0;i<goalset->size();i++) {
		Constraint *var=goalvararray[i];
		solver->freeze(var->getVar());
	}

	clock_t start=myclock();
	bool *solution=runSolver();
	clock_t finish=myclock();
#ifdef STATS
	if (curr_stat != NULL)
		stats->push_back(curr_stat);
	curr_stat=new MC_Stat();
	curr_stat->time=finish-start;
	curr_stat->was_incremental=false;
	curr_stat->was_sat=(solution!=NULL);
	curr_stat->fgoals=0;
	curr_stat->bgoals=0;

	if (solution == NULL) {
		stats->push_back(curr_stat);
		for(uint i=0;i<stats->size();i++) {
			struct MC_Stat *s=(*stats)[i];
			model_print("time=%lld fgoals=%d bgoals=%d  incremental=%d sat=%d\n", s->time, s->fgoals, s->bgoals, s->was_incremental, s->was_sat);
		}

	}
#endif

	model_print("start %lu, finish %lu\n", start,finish);
	model_print("Initial time = %12.6f\n", ((double)(finish-start))/1000000);


	if (solution==NULL) {
		delete solver;
		solver = NULL;
		return true;
	}

	//Zero out the achieved goals
	for(uint i=0;i<goalset->size();i++) {
		Constraint *var=goalvararray[i];
		if (var != NULL) {
			if (solution[var->getVar()]) {
				//if (goalvararray[i] != NULL)
				//	model_print("SAT clearing goal %d.\n", i);
				goalvararray[i]=NULL;
			}
		}
	}

	schedulebuilder->buildSchedule(solution);
	model_free(solution);
	return false;
}

void ConstGen::printEventGraph() {
	char buffer[128];
	sprintf(buffer, "eventgraph%u.dot",schedule_graph);
	schedule_graph++;
	int file=open(buffer,O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
	model_dprintf(file, "digraph eventgraph {\n");

	EPRecord *first=execution->getEPRecord(MCID_FIRST);
	printRecord(first, file);
	while(!workstack->empty()) {
		EPRecord *record=workstack->back();
		workstack->pop_back();
		printRecord(record, file);
	}

	model_dprintf(file, "}\n");
	close(file);
}

void ConstGen::doPrint(EPRecord *record, int file) {
	model_dprintf(file, "%lu[label=\"",(uintptr_t)record);
	record->print(file);
	model_dprintf(file, "\"];\n");
	if (record->getNextRecord()!=NULL)
		model_dprintf(file, "%lu->%lu;\n", (uintptr_t) record, (uintptr_t) record->getNextRecord());

	if (record->getChildRecord()!=NULL)
		model_dprintf(file, "%lu->%lu[color=red];\n", (uintptr_t) record, (uintptr_t) record->getChildRecord());
}

void ConstGen::printRecord(EPRecord *record, int file) {
	do {
		doPrint(record,file);

		if (record->getType()==LOOPENTER) {
			if (record->getNextRecord()!=NULL)
				workstack->push_back(record->getNextRecord());
			workstack->push_back(record->getChildRecord());
			return;
		}
		if (record->getChildRecord()!=NULL) {
			workstack->push_back(record->getChildRecord());
		}
		if (record->getType()==NONLOCALTRANS) {
			return;
		}
		if (record->getType()==LOOPEXIT) {
			return;
		}
		if (record->getType()==BRANCHDIR) {
			EPRecord *next=record->getNextRecord();
			if (next != NULL)
				workstack->push_back(next);
			for(uint i=0;i<record->numValues();i++) {
				EPValue *branchdir=record->getValue(i);

				//Could have an empty branch, so make sure the branch actually
				//runs code
				if (branchdir->getFirstRecord()!=NULL) {
					workstack->push_back(branchdir->getFirstRecord());
					model_dprintf(file, "%lu->%lu[color=blue];\n", (uintptr_t) record, (uintptr_t) branchdir->getFirstRecord());
				}
			}
			return;
		} else
			record=record->getNextRecord();
	} while(record!=NULL);
}


/* This function traverses a thread's graph in execution order.  */

void ConstGen::traverse(bool initial) {
	EPRecord *first=execution->getEPRecord(MCID_FIRST);
	traverseRecord(first, initial);
	while(!workstack->empty()) {
		EPRecord *record=workstack->back();
		workstack->pop_back();
		traverseRecord(record, initial);
	}
}

/** This method looks for all other memory operations that could
                potentially share a location, and build partitions of memory
                locations such that all memory locations that can share the same
                address are in the same group.

                These memory operations should share an encoding of addresses and
                values.
 */

void ConstGen::groupMemoryOperations(EPRecord *op) {
	/** Handle our first address */
	IntHashSet *addrset=op->getSet(VC_ADDRINDEX);
	IntIterator *ait=addrset->iterator();

	void *iaddr=(void *) ait->next();
	StoreLoadSet *isls=storeloadtable->get(iaddr);
	if (isls==NULL) {
		isls=new StoreLoadSet();
		storeloadtable->put(iaddr, isls);
	}
	isls->add(op);

	while(ait->hasNext()) {
		void *addr=(void *)ait->next();
		StoreLoadSet *sls=storeloadtable->get(addr);
		if (sls==NULL) {
			storeloadtable->put(addr, isls);
		} else if (sls!=isls) {
			//must do merge
			mergeSets(isls, sls);
		}
	}
	delete ait;
}


RecordSet * ConstGen::computeConflictSet(EPRecord *store, EPRecord *load, RecordSet * baseset) {
	//See if we can determine addresses that store/load may use
	IntIterator * storeaddr=store->getSet(VC_ADDRINDEX)->iterator();
	int numaddr=0;void *commonaddr=NULL;
	while(storeaddr->hasNext()) {
		void *addr=(void *) storeaddr->next();
		if (load->getSet(VC_ADDRINDEX)->contains((uint64_t)addr)) {
			commonaddr=addr;
			numaddr++;
		}
	}
	delete storeaddr;

	ASSERT(numaddr!=0);
	if (numaddr!=1)
		return NULL;

	RecordSet *srscopy=baseset->copy();
	RecordIterator *sri=srscopy->iterator();
	while(sri->hasNext()) {
		bool pruneconflictstore=false;
		EPRecord *conflictstore=sri->next();
		bool conflictafterstore=getOrderConstraint(store,conflictstore)->isTrue();
		bool conflictbeforeload=getOrderConstraint(conflictstore,load)->isTrue();

		if (conflictafterstore) {
			RecordIterator *sricheck=srscopy->iterator();
			while(sricheck->hasNext()) {
				EPRecord *checkstore=sricheck->next();
				bool checkafterstore=getOrderConstraint(store, checkstore)->isTrue();
				if (!checkafterstore)
					continue;
				bool checkbeforeconflict=getOrderConstraint(checkstore,conflictstore)->isTrue();
				if (!checkbeforeconflict)
					continue;

				//See if the checkstore must store to the relevant address
				IntHashSet * storeaddr=checkstore->getSet(VC_ADDRINDEX);

				if (storeaddr->getSize()!=1 || !storeaddr->contains((uint64_t)commonaddr))
					continue;


				if (subsumesExecutionConstraint(checkstore, conflictstore)) {
					pruneconflictstore=true;
					break;
				}
			}
			delete sricheck;
		}

		if (conflictbeforeload && !pruneconflictstore) {
			RecordIterator *sricheck=srscopy->iterator();
			while(sricheck->hasNext()) {
				EPRecord *checkstore=sricheck->next();

				bool checkafterconflict=getOrderConstraint(conflictstore, checkstore)->isTrue();
				if (!checkafterconflict)
					continue;

				bool checkbeforeload=getOrderConstraint(checkstore, load)->isTrue();
				if (!checkbeforeload)
					continue;

				//See if the checkstore must store to the relevant address
				IntHashSet * storeaddr=checkstore->getSet(VC_ADDRINDEX);
				if (storeaddr->getSize()!=1 || !storeaddr->contains((uint64_t)commonaddr))
					continue;


				if (subsumesExecutionConstraint(checkstore, conflictstore)) {
					pruneconflictstore=true;
					break;
				}
			}
			delete sricheck;
		}
		if (pruneconflictstore) {
			//This store is redundant
			sri->remove();
		}
	}

	delete sri;
	return srscopy;
}


/** This method returns all stores that a load may read from. */

RecordSet * ConstGen::getMayReadFromSet(EPRecord *load) {
	if (load->getSet(VC_ADDRINDEX)->getSize()==1)
		return getMayReadFromSetOpt(load);

	RecordSet *srs=loadtable->get(load);
	ExecPoint *epload=load->getEP();
	thread_id_t loadtid=epload->get_tid();
	if (srs==NULL) {
		srs=new RecordSet();
		loadtable->put(load, srs);

		IntHashSet *addrset=load->getSet(VC_ADDRINDEX);
		IntIterator *ait=addrset->iterator();

		while(ait->hasNext()) {
			void *addr=(void *)ait->next();
			RecordSet *rs=execution->getStoreTable(addr);
			if (rs==NULL)
				continue;

			RecordIterator * rit=rs->iterator();
			while(rit->hasNext()) {
				EPRecord *rec=rit->next();
				ExecPoint *epstore=rec->getEP();
				thread_id_t storetid=epstore->get_tid();
				if (storetid != loadtid ||
						epstore->compare(epload) == CR_AFTER)
					srs->add(rec);
			}
			delete rit;
		}
		delete ait;
	}
	return srs;
}


RecordSet * ConstGen::getMayReadFromSetOpt(EPRecord *load) {
	RecordSet *srs=loadtable->get(load);
	ExecPoint *epload=load->getEP();
	thread_id_t loadtid=epload->get_tid();
	if (srs==NULL) {
		srs=new RecordSet();
		loadtable->put(load, srs);

		IntHashSet *addrset=load->getSet(VC_ADDRINDEX);
		IntIterator *ait=addrset->iterator();
		void *addr=(void *)ait->next();
		delete ait;

		RecordSet *rs=execution->getStoreTable(addr);
		RecordIterator * rit=rs->iterator();
		ExecPoint *latest=NULL;
		while(rit->hasNext()) {
			EPRecord *store=rit->next();
			ExecPoint *epstore=store->getEP();
			thread_id_t storetid=epstore->get_tid();
			if (storetid == loadtid && isAlwaysExecuted(store) &&
					store->getSet(VC_ADDRINDEX)->getSize()==1 &&
					epstore->compare(epload) == CR_AFTER &&
					(latest==NULL || latest->compare(epstore) == CR_AFTER)) {
				latest=epstore;
			}
		}
		delete rit;
		rit=rs->iterator();
		while(rit->hasNext()) {
			EPRecord *store=rit->next();
			ExecPoint *epstore=store->getEP();
			thread_id_t storetid=epstore->get_tid();
			if (storetid == loadtid) {
				if (epstore->compare(epload) == CR_AFTER &&
						(latest==NULL || epstore->compare(latest) != CR_AFTER))
					srs->add(store);
			} else {
				srs->add(store);
			}
		}
		delete rit;
	}
	return srs;
}

/** This function merges two recordsets and updates the storeloadtable
                accordingly. */

void ConstGen::mergeSets(StoreLoadSet *to, StoreLoadSet *from) {
	RecordIterator * sri=from->iterator();
	while(sri->hasNext()) {
		EPRecord *rec=sri->next();
		to->add(rec);
		IntHashSet *addrset=rec->getSet(VC_ADDRINDEX);
		IntIterator *ait=addrset->iterator();
		while(ait->hasNext()) {
			void *addr=(void *)ait->next();
			storeloadtable->put(addr, to);
		}
		delete ait;
	}
	delete sri;
	delete from;
}

#ifdef TSO
/** This function creates ordering variables between stores and loads
 *  in same thread for TSO.  */

void ConstGen::insertTSOAction(EPRecord *load) {
	if (load->getType() != LOAD)
		return;
	ExecPoint *load_ep=load->getEP();
	thread_id_t tid=load_ep->get_tid();
	uint thread=id_to_int(tid);
	ModelVector<EPRecord *> * vector=(*threadactions)[thread];
	uint j=vector->size()-1;
	while (j>0) {
		EPRecord *oldrec=(*vector)[--j];
		EventType oldrec_t=oldrec->getType();

		if (((oldrec_t == RMW) || (oldrec_t==FENCE)) &&
				isAlwaysExecuted(oldrec)) {
			return;
		} else if (oldrec_t == STORE) {
			/* Only generate variables for things that can actually both run */

			createOrderConstraint(oldrec, load);
		}
	}
}

void ConstGen::genTSOTransOrderConstraints() {
	for(uint t1=0;t1<threadactions->size();t1++) {
		ModelVector<EPRecord *> *tvec=(*threadactions)[t1];
		uint size=tvec->size();
		EPRecord *laststore=NULL;
		for(uint store_i=0;store_i<size;store_i++) {
			EPRecord *store=(*tvec)[store_i];
			EventType store_t=store->getType();
			if (store_t != STORE)
				continue;
			EPRecord *lastload=NULL;
			for(uint load_i=store_i+1;load_i<size;load_i++) {
				EPRecord *rec=(*tvec)[store_i];
				//Hit fence, don't need more stuff
				EventType rec_t=rec->getType();
				if (((rec_t == RMW) || (rec_t == FENCE)) &&
						isAlwaysExecuted(rec))
					break;
				if (rec_t != LOAD)
					continue;
				if (laststore != NULL) {
					Constraint * storeload=getOrderConstraint(store, rec);
					Constraint * earlystoreload=getOrderConstraint(laststore, rec);
					Constraint * c=new Constraint(IMPLIES, storeload, earlystoreload);
					ADDCONSTRAINT(c, "earlystore");
				}
				if (lastload != NULL) {
					Constraint * storeearlyload=getOrderConstraint(store, lastload);
					Constraint * storeload=getOrderConstraint(store, rec);
					Constraint * c=new Constraint(IMPLIES, storeearlyload, storeload);
					ADDCONSTRAINT(c, "earlyload");
				}
				lastload=rec;
			}
			laststore=store;
		}
	}
}
#endif

/** This function creates ordering constraints to implement SC for
                memory operations.  */

void ConstGen::insertAction(EPRecord *record) {
	thread_id_t tid=record->getEP()->get_tid();
	uint thread=id_to_int(tid);
	if (threadactions->size()<=thread) {
		uint oldsize=threadactions->size();
		threadactions->resize(thread+1);
		for(;oldsize<=thread;oldsize++) {
			(*threadactions)[oldsize]=new ModelVector<EPRecord *>();
		}
	}

	(*threadactions)[thread]->push_back(record);

	for(uint i=0;i<threadactions->size();i++) {
		if (i==thread)
			continue;
		ModelVector<EPRecord *> * vector=(*threadactions)[i];
		for(uint j=0;j<vector->size();j++) {
			EPRecord *oldrec=(*vector)[j];
			createOrderConstraint(oldrec, record);
		}
	}
#ifdef TSO
	insertTSOAction(record);
#endif
}

void ConstGen::genTransOrderConstraints() {
	for(uint t1=0;t1<threadactions->size();t1++) {
		ModelVector<EPRecord *> *t1vec=(*threadactions)[t1];
		for(uint t2=0;t2<t1;t2++) {
			ModelVector<EPRecord *> *t2vec=(*threadactions)[t2];
			genTransOrderConstraints(t1vec, t2vec);
			genTransOrderConstraints(t2vec, t1vec);
			for(uint t3=0;t3<t2;t3++) {
				ModelVector<EPRecord *> *t3vec=(*threadactions)[t3];
				genTransOrderConstraints(t1vec, t2vec, t3vec);
			}
		}
	}
}

void ConstGen::genTransOrderConstraints(ModelVector<EPRecord *> * t1vec, ModelVector<EPRecord *> *t2vec) {
	for(uint t1i=0;t1i<t1vec->size();t1i++) {
		EPRecord *t1rec=(*t1vec)[t1i];
		for(uint t2i=0;t2i<t2vec->size();t2i++) {
			EPRecord *t2rec=(*t2vec)[t2i];

			/* Note: One only really needs to generate the first constraint
			         in the first loop and the last constraint in the last loop.
			         I tried this and performance suffered on linuxrwlocks and
			         linuxlocks at the current time. BD - August 2014*/
			Constraint *c21=getOrderConstraint(t2rec, t1rec);

			/* short circuit for the trivial case */
			if (c21->isTrue())
				continue;

			for(uint t3i=t2i+1;t3i<t2vec->size();t3i++) {
				EPRecord *t3rec=(*t2vec)[t3i];
				Constraint *c13=getOrderConstraint(t1rec, t3rec);
#ifdef TSO
				Constraint *c32=getOrderConstraint(t3rec, t2rec);
				if (!c32->isFalse()) {
					//Have a store->load that could be reordered, need to generate other constraint
					Constraint *c12=getOrderConstraint(t1rec, t2rec);
					Constraint *c23=getOrderConstraint(t2rec, t3rec);
					Constraint *c31=getOrderConstraint(t3rec, t1rec);
					Constraint *slarray[] = {c31, c23, c12};
					Constraint * intradelayedstore=new Constraint(OR, 3, slarray);
					ADDCONSTRAINT(intradelayedstore, "intradelayedstore");
				}
				Constraint * array[]={c21, c32, c13};
				Constraint *intratransorder=new Constraint(OR, 3, array);
#else
				Constraint *intratransorder=new Constraint(OR, c21, c13);
#endif
				ADDCONSTRAINT(intratransorder,"intratransorder");
			}

			for(uint t0i=0;t0i<t1i;t0i++) {
				EPRecord *t0rec=(*t1vec)[t0i];
				Constraint *c02=getOrderConstraint(t0rec, t2rec);
#ifdef TSO
				Constraint *c10=getOrderConstraint(t1rec, t0rec);

				if (!c10->isFalse()) {
					//Have a store->load that could be reordered, need to generate other constraint
					Constraint *c01=getOrderConstraint(t0rec, t1rec);
					Constraint *c12=getOrderConstraint(t1rec, t2rec);
					Constraint *c20=getOrderConstraint(t2rec, t0rec);
					Constraint *slarray[] = {c01, c12, c20};
					Constraint * intradelayedstore=new Constraint(OR, 3, slarray);
					ADDCONSTRAINT(intradelayedstore, "intradelayedstore");
				}
				Constraint * array[]={c10, c21, c02};
				Constraint *intratransorder=new Constraint(OR, 3, array);
#else
				Constraint *intratransorder=new Constraint(OR, c21, c02);
#endif
				ADDCONSTRAINT(intratransorder,"intratransorder");
			}
		}
	}
}


void ConstGen::genTransOrderConstraints(ModelVector<EPRecord *> * t1vec, ModelVector<EPRecord *> *t2vec, ModelVector<EPRecord *> * t3vec) {
	for(uint t1i=0;t1i<t1vec->size();t1i++) {
		EPRecord *t1rec=(*t1vec)[t1i];
		for(uint t2i=0;t2i<t2vec->size();t2i++) {
			EPRecord *t2rec=(*t2vec)[t2i];
			for(uint t3i=0;t3i<t3vec->size();t3i++) {
				EPRecord *t3rec=(*t3vec)[t3i];
				genTransOrderConstraint(t1rec, t2rec, t3rec);
			}
		}
	}
}

void ConstGen::genTransOrderConstraint(EPRecord *t1rec, EPRecord *t2rec, EPRecord *t3rec) {
	Constraint *c21=getOrderConstraint(t2rec, t1rec);
	Constraint *c32=getOrderConstraint(t3rec, t2rec);
	Constraint *c13=getOrderConstraint(t1rec, t3rec);
	Constraint * cimpl1[]={c21, c32, c13};
	Constraint * c1=new Constraint(OR, 3, cimpl1);
	ADDCONSTRAINT(c1, "intertransorder");

	Constraint *c12=getOrderConstraint(t1rec, t2rec);
	Constraint *c23=getOrderConstraint(t2rec, t3rec);
	Constraint *c31=getOrderConstraint(t3rec, t1rec);
	Constraint * cimpl2[]={c12, c23, c31};
	Constraint *c2=new Constraint(OR, 3, cimpl2);
	ADDCONSTRAINT(c2, "intertransorder");
}

void ConstGen::addGoal(EPRecord *r, Constraint *c) {
	rectoint->put(r, goalset->size());
	goalset->push_back(c);
}

void ConstGen::addBranchGoal(EPRecord *r, Constraint *c) {
	has_untaken_branches=true;
	rectoint->put(r, goalset->size());
	goalset->push_back(c);
}

void ConstGen::achievedGoal(EPRecord *r) {
	if (rectoint->contains(r)) {
		uint index=rectoint->get(r);
		//if (goalvararray[index] != NULL)
		//model_print("Run Clearing goal index %d\n",index);
		goalvararray[index]=NULL;
	}
}

void ConstGen::addConstraint(Constraint *c) {
	ModelVector<Constraint *> *vec=c->simplify();
	for(uint i=0;i<vec->size();i++) {
		Constraint *simp=(*vec)[i];
		if (simp->type==TRUE)
			continue;
		ASSERT(simp->type!=FALSE);
		simp->printDIMACS(this);
#ifdef VERBOSE_CONSTRAINTS
		simp->print();
		model_print("\n");
#endif
		numconstraints++;
		simp->freerec();
	}
	delete vec;
}

void ConstGen::printNegConstraint(uint value) {
	int val=-value;
	solver->addClauseLiteral(val);
}

void ConstGen::printConstraint(uint value) {
	solver->addClauseLiteral(value);
}

bool * ConstGen::runSolver() {
	int solution=solver->solve();
	if (solution == IS_UNSAT) {
		return NULL;
	} else if (solution == IS_SAT) {
		bool * assignments=(bool *)model_malloc(sizeof(bool)*(varindex+1));
		for(uint i=0;i<=varindex;i++)
			assignments[i]=solver->getValue(i);
		return assignments;
	} else {
		delete solver;
		solver=NULL;
		model_print_err("INDETER\n");
		model_print("INDETER\n");
		exit(-1);
		return NULL;
	}
}

Constraint * ConstGen::getOrderConstraint(EPRecord *first, EPRecord *second) {
#ifndef TSO
	if (first->getEP()->get_tid()==second->getEP()->get_tid()) {
		if (first->getEP()->compare(second->getEP())==CR_AFTER)
			return &ctrue;
		else {
			return &cfalse;
		}
	}
#endif
	RecPair rp(first, second);
	RecPair *rpc=rpt->get(&rp);
#ifdef TSO
	if ((rpc==NULL) &&
			first->getEP()->get_tid()==second->getEP()->get_tid()) {
		if (first->getEP()->compare(second->getEP())==CR_AFTER)
			return &ctrue;
		else {
			return &cfalse;
		}
	}
#endif
	ASSERT(rpc!=NULL);
	//	delete rp;
	Constraint *c=rpc->constraint;
	if (rpc->epr1!=first) {
		//have to flip arguments
		return c->negate();
	} else {
		return c;
	}
}

bool ConstGen::getOrder(EPRecord *first, EPRecord *second, bool * satsolution) {
	RecPair rp(first, second);
	RecPair *rpc=rpt->get(&rp);
#ifdef TSO
	if ((rpc==NULL) &&
			first->getEP()->get_tid()==second->getEP()->get_tid()) {
		if (first->getEP()->compare(second->getEP())==CR_AFTER)
			return true;
		else {
			return false;
		}
	}
#endif

	ASSERT(rpc!=NULL);

	Constraint *c=rpc->constraint;
	CType type=c->getType();
	bool order;

	if (type==TRUE)
		order=true;
	else if (type==FALSE)
		order=false;
	else {
		uint index=c->getVar();
		order=satsolution[index];
	}
	if (rpc->epr1==first)
		return order;
	else
		return !order;
}

/** This function determines whether events first and second are
 * ordered by start and join operations.  */

bool ConstGen::orderThread(EPRecord *first, EPRecord *second) {
	ExecPoint * ep1=first->getEP();
	ExecPoint * ep2=second->getEP();
	thread_id_t thr1=ep1->get_tid();
	thread_id_t thr2=ep2->get_tid();
	Thread *tr2=execution->get_thread(thr2);
	EPRecord *thr2start=tr2->getParentRecord();
	if (thr2start!=NULL) {
		ExecPoint *epthr2start=thr2start->getEP();
		if (epthr2start->get_tid()==thr1 &&
				ep1->compare(epthr2start)==CR_AFTER)
			return true;
	}
	ModelVector<EPRecord *> * joinvec=execution->getJoins();
	for(uint i=0;i<joinvec->size();i++) {
		EPRecord *join=(*joinvec)[i];
		ExecPoint *jp=join->getEP();
		if (jp->get_tid()==thr2 &&
				jp->compare(ep2)==CR_AFTER &&
				join->getJoinThread() == thr1)
			return true;
	}
	return false;
}

/** This function generates an ordering constraint for two events.  */

void ConstGen::createOrderConstraint(EPRecord *first, EPRecord *second) {
	RecPair * rp=new RecPair(first, second);
	if (!rpt->contains(rp)) {
		if (orderThread(first, second))
			rp->constraint=&ctrue;
		else if (orderThread(second, first))
			rp->constraint=&cfalse;
		else
			rp->constraint=getNewVar();

		rpt->put(rp, rp);
	} else {
		delete rp;
	}
}

Constraint * ConstGen::getNewVar() {
	Constraint *var;
	if (varindex>vars->size()) {
		var=new Constraint(VAR, varindex);
		Constraint *varneg=new Constraint(NOTVAR, varindex);
		var->setNeg(varneg);
		varneg->setNeg(var);
		vars->push_back(var);
	} else {
		var=(*vars)[varindex-1];
	}
	varindex++;
	return var;
}

/** Gets an array of new variables.  */

void ConstGen::getArrayNewVars(uint num, Constraint **carray) {
	for(uint i=0;i<num;i++)
		carray[i]=getNewVar();
}

StoreLoadSet * ConstGen::getStoreLoadSet(EPRecord *record) {
	IntIterator *it=record->getSet(VC_ADDRINDEX)->iterator();
	void *addr=(void *)it->next();
	delete it;
	return storeloadtable->get(addr);
}

/** Returns a constraint that is true if the output of record has the
                given value. */

Constraint * ConstGen::getRetValueEncoding(EPRecord *record, uint64_t value) {
	switch(record->getType()) {
	case EQUALS:
		return equalstable->get(record)->getValueEncoding(value);
	case FUNCTION:
		return functiontable->get(record)->getValueEncoding(value);
	case LOAD: {
		return getStoreLoadSet(record)->getValueEncoding(this, record, value);
	}
	case RMW: {
		return getStoreLoadSet(record)->getRMWRValueEncoding(this, record, value);
	}
	default:
		ASSERT(false);
		exit(-1);
	}
}

Constraint * ConstGen::getMemValueEncoding(EPRecord *record, uint64_t value) {
	switch(record->getType()) {
	case STORE:
		return getStoreLoadSet(record)->getValueEncoding(this, record, value);
	case RMW:
		return getStoreLoadSet(record)->getValueEncoding(this, record, value);
	default:
		ASSERT(false);
		exit(-1);
	}
}

/** Return true if the execution of record implies the execution of
 *	recordsubsumes */

bool ConstGen::subsumesExecutionConstraint(EPRecord *recordsubsumes, EPRecord *record) {
	EPValue *branch=record->getBranch();
	EPValue *branchsubsumes=recordsubsumes->getBranch();
	if (branchsubsumes != NULL) {
		bool branchsubsumed=false;
		while(branch!=NULL) {
			if (branchsubsumes==branch) {
				branchsubsumed=true;
				break;
			}
			branch=branch->getRecord()->getBranch();
		}
		if (!branchsubsumed)
			return false;
	}
	RecordSet *srs=execcondtable->get(recordsubsumes);

	if (srs!=NULL) {
		RecordIterator *sri=srs->iterator();
		while(sri->hasNext()) {
			EPRecord *rec=sri->next();

			if (!getOrderConstraint(rec, record)->isTrue()) {
				delete sri;
				return false;
			}
		}
		delete sri;
	}
	return true;
}

Constraint * ConstGen::getExecutionConstraint(EPRecord *record) {
	EPValue *branch=record->getBranch();
	RecordSet *srs=execcondtable->get(record);
	int size=srs==NULL ? 0 : srs->getSize();
	if (branch!=NULL)
		size++;

	Constraint *array[size];
	int index=0;
	if (srs!=NULL) {
		RecordIterator *sri=srs->iterator();
		while(sri->hasNext()) {
			EPRecord *rec=sri->next();
			EPValue *recbranch=rec->getBranch();
			BranchRecord *guardbr=branchtable->get(recbranch->getRecord());
			array[index++]=guardbr->getBranch(recbranch)->negate();
		}
		delete sri;
	}
	if (branch!=NULL) {
		BranchRecord *guardbr=branchtable->get(branch->getRecord());
		array[index++]=guardbr->getBranch(branch);
	}
	if (index==0)
		return &ctrue;
	else if (index==1)
		return array[0];
	else
		return new Constraint(AND, index, array);
}

bool ConstGen::isAlwaysExecuted(EPRecord *record) {
	EPValue *branch=record->getBranch();
	RecordSet *srs=execcondtable->get(record);
	int size=srs==NULL ? 0 : srs->getSize();
	if (branch!=NULL)
		size++;

	return size==0;
}

void ConstGen::insertBranch(EPRecord *record) {
	uint numvalue=record->numValues();
	/** need one value for new directions */
	if (numvalue<record->getLen()) {
		numvalue++;
		if (model->params.noexecyields) {
			incompleteexploredbranch->add(record);
		}
	}
	/** need extra value to represent that branch wasn't executed. **/
	bool alwaysexecuted=isAlwaysExecuted(record);
	if (!alwaysexecuted)
		numvalue++;
	uint numbits=NUMBITS(numvalue-1);
	Constraint *bits[numbits];
	getArrayNewVars(numbits, bits);
	BranchRecord *br=new BranchRecord(record, numbits, bits, alwaysexecuted);
	branchtable->put(record, br);
}

void ConstGen::processBranch(EPRecord *record) {
	BranchRecord *br=branchtable->get(record);
	if (record->numValues()<record->getLen()) {
		Constraint *goal=br->getNewBranch();
		ADDBRANCHGOAL(record, goal,"newbranch");
	}

	/** Insert criteria of parent branch going the right way. **/
	Constraint *baseconstraint=getExecutionConstraint(record);

	if (!isAlwaysExecuted(record)) {
		Constraint *parentbranch=new Constraint(IMPLIES, br->getAnyBranch(), baseconstraint);
		ADDCONSTRAINT(parentbranch, "parentbranch");
	}

	/** Insert criteria for directions */
	ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, VC_BASEINDEX);
	ASSERT(depvec->size()==1);
	EPRecord * val_record=(*depvec)[0];
	for(unsigned int i=0;i<record->numValues();i++) {
		EPValue * branchval=record->getValue(i);
		uint64_t val=branchval->getValue();

		if (val==0) {
			Constraint *execconstraint=getExecutionConstraint(record);
			Constraint *br_false=new Constraint(IMPLIES,
																					new Constraint(AND, execconstraint,
																												 getRetValueEncoding(val_record, val)), br->getBranch(branchval));
			ADDCONSTRAINT(br_false, "br_false");
		} else {
			if (record->getBranchAnyValue()) {
				if (getRetValueEncoding(val_record, 0)!=NULL) {
					Constraint *execconstraint=getExecutionConstraint(record);
					Constraint *br_true1=new Constraint(IMPLIES, new Constraint(AND, getRetValueEncoding(val_record, 0)->negate(),
																																			execconstraint),
																							br->getBranch(branchval));
					ADDCONSTRAINT(br_true1, "br_true1");
				} else {
					for(unsigned int j=0;j<val_record->numValues();j++) {
						EPValue * epval=val_record->getValue(j);
						Constraint *execconstraint=getExecutionConstraint(record);
						Constraint *valuematches=getRetValueEncoding(val_record, epval->getValue());
						Constraint *br_true2=new Constraint(IMPLIES, new Constraint(AND, execconstraint, valuematches), br->getBranch(branchval));
						ADDCONSTRAINT(br_true2, "br_true2");
					}
				}
			} else {
				Constraint *execconstraint=getExecutionConstraint(record);
				Constraint *br_val=new Constraint(IMPLIES, new Constraint(AND, execconstraint, getRetValueEncoding(val_record, val)), br->getBranch(branchval));
				ADDCONSTRAINT(br_val, "br_val");
			}
		}
	}
}

void ConstGen::insertFunction(EPRecord *record) {
	FunctionRecord * fr=new FunctionRecord(this, record);
	functiontable->put(record, fr);
}

void ConstGen::insertEquals(EPRecord *record) {
	EqualsRecord * fr=new EqualsRecord(this, record);
	equalstable->put(record, fr);
}

void ConstGen::processLoad(EPRecord *record) {
	LoadRF * lrf=new LoadRF(record, this);
	lrf->genConstraints(this);
	delete lrf;
	processAddresses(record);
}

/** This procedure generates the constraints that set the address
                variables for load/store/rmw operations. */

void ConstGen::processAddresses(EPRecord *record) {
	StoreLoadSet *sls=getStoreLoadSet(record);
	ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, VC_ADDRINDEX);
	if (depvec==NULL) {
		//we have a hard coded address
		const void *addr=record->getValue(0)->getAddr();
		Constraint *addrenc=sls->getAddressEncoding(this, record, addr);
		ADDCONSTRAINT(addrenc,"fixedaddress");
	} else {
		//we take as input an address and have to generate implications
		//for each possible input address
		ASSERT(depvec->size()==1);
		EPRecord *src=(*depvec)[0];
		IntIterator *it=record->getSet(VC_ADDRINDEX)->iterator();

		uintptr_t offset=record->getOffset();

		while(it->hasNext()) {
			uint64_t addr=it->next();
			Constraint *srcenc=getRetValueEncoding(src, addr-offset);
			Constraint *addrenc=sls->getAddressEncoding(this, record, (void *) addr);
			Constraint *addrmatch=new Constraint(IMPLIES, srcenc, addrenc);
			ADDCONSTRAINT(addrmatch,"setaddress");
		}
		delete it;
	}
}

void ConstGen::processCAS(EPRecord *record) {
	//First do the load
	LoadRF * lrf=new LoadRF(record, this);
	lrf->genConstraints(this);
	delete lrf;
	//Next see if we are successful
	Constraint *eq=getNewVar();
	ModelVector<EPRecord *> * depveccas=execution->getRevDependences(record, VC_OLDVALCASINDEX);
	if (depveccas==NULL) {
		//Hard coded old value
		IntIterator *iit=record->getSet(VC_OLDVALCASINDEX)->iterator();
		uint64_t valcas=iit->next();
		delete iit;
		Constraint *rmwr=getStoreLoadSet(record)->getRMWRValueEncoding(this, record, valcas);
		if (rmwr==NULL) {
			Constraint *cascond=eq->negate();
			ADDCONSTRAINT(cascond, "cascond");
		} else {
			Constraint *cascond=generateEquivConstraint(eq, rmwr);
			ADDCONSTRAINT(cascond, "cascond");
		}
	} else {
		ASSERT(depveccas->size()==1);
		EPRecord *src=(*depveccas)[0];
		IntIterator *it=getStoreLoadSet(record)->getValues()->iterator();

		while(it->hasNext()) {
			uint64_t valcas=it->next();
			Constraint *srcenc=getRetValueEncoding(src, valcas);
			Constraint *storeenc=getStoreLoadSet(record)->getRMWRValueEncoding(this, record, valcas);

			if (srcenc!=NULL && storeenc!=NULL) {
				Constraint *cond=new Constraint(AND,
																				new Constraint(IMPLIES, srcenc->clone(), eq),
																				new Constraint(IMPLIES, eq, srcenc));
				Constraint *cas=new Constraint(IMPLIES, storeenc, cond);
				ADDCONSTRAINT(cas, "cas");
			} else if (srcenc==NULL) {
				Constraint *casfail=new Constraint(IMPLIES, storeenc, eq->negate());
				ADDCONSTRAINT(casfail,"casfail_eq");
			} else {
				//srcenc must be non-null and store-encoding must be null
				srcenc->free();
			}
		}
		delete it;
		IntIterator *iit=record->getSet(VC_OLDVALCASINDEX)->iterator();
		while(iit->hasNext()) {
			uint64_t val=iit->next();
			if (!getStoreLoadSet(record)->getValues()->contains(val)) {
				Constraint *srcenc=getRetValueEncoding(src, val);
				Constraint *casfail=new Constraint(IMPLIES, srcenc, eq->negate());
				ADDCONSTRAINT(casfail,"casfailretval");
			}
		}
		delete iit;
	}

	ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, VC_BASEINDEX);
	if (depvec==NULL) {
		IntIterator *iit=record->getSet(VC_BASEINDEX)->iterator();
		uint64_t val=iit->next();
		delete iit;
		Constraint *storeenc=getMemValueEncoding(record, val);
		Constraint *casmemsuc=new Constraint(IMPLIES, eq, storeenc);
		ADDCONSTRAINT(casmemsuc, "casmemsuc");
	} else {
		ASSERT(depvec->size()==1);
		EPRecord *src=(*depvec)[0];
		IntIterator *it=record->getStoreSet()->iterator();

		while(it->hasNext()) {
			uint64_t val=it->next();
			Constraint *srcenc=getRetValueEncoding(src, val);
			if (srcenc==NULL) {
				//this can happen for values that are in the store set because
				//we re-stored them on a failed CAS
				continue;
			}
			Constraint *storeenc=getMemValueEncoding(record, val);
			Constraint *storevalue=new Constraint(IMPLIES, new Constraint(AND, eq, srcenc), storeenc);
			ADDCONSTRAINT(storevalue,"casmemsuc");
		}
		delete it;
	}
	StoreLoadSet *sls=getStoreLoadSet(record);

	Constraint *repeatval=generateEquivConstraint(sls->getNumValVars(), sls->getValVars(this, record), sls->getRMWRValVars(this, record));
	Constraint *failcas=new Constraint(IMPLIES, eq->negate(), repeatval);
	ADDCONSTRAINT(failcas,"casmemfail");

	processAddresses(record);
}

void ConstGen::processEXC(EPRecord *record) {
	//First do the load
	LoadRF * lrf=new LoadRF(record, this);
	lrf->genConstraints(this);
	delete lrf;

	ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, VC_BASEINDEX);
	if (depvec==NULL) {
		IntIterator *iit=record->getSet(VC_BASEINDEX)->iterator();
		uint64_t val=iit->next();
		delete iit;
		Constraint *storeenc=getMemValueEncoding(record, val);
		ADDCONSTRAINT(storeenc, "excmemsuc");
	} else {
		ASSERT(depvec->size()==1);
		EPRecord *src=(*depvec)[0];
		IntIterator *it=record->getStoreSet()->iterator();

		while(it->hasNext()) {
			uint64_t val=it->next();
			Constraint *srcenc=getRetValueEncoding(src, val);
			Constraint *storeenc=getMemValueEncoding(record, val);
			Constraint *storevalue=new Constraint(IMPLIES, srcenc, storeenc);
			ADDCONSTRAINT(storevalue,"excmemsuc");
		}
		delete it;
	}

	processAddresses(record);
}

void ConstGen::processAdd(EPRecord *record) {
	//First do the load
	LoadRF * lrf=new LoadRF(record, this);
	lrf->genConstraints(this);
	delete lrf;
	Constraint *var=getNewVar();
	Constraint *newadd=new Constraint(AND, var, getExecutionConstraint(record));
	ADDGOAL(record, newadd, "newadd");

	ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, VC_BASEINDEX);
	if (depvec==NULL) {
		IntIterator *valit=record->getSet(VC_BASEINDEX)->iterator();
		uint64_t val=valit->next();
		delete valit;
		IntHashSet *valset=getStoreLoadSet(record)->getValues();
		IntIterator *sis=valset->iterator();
		while(sis->hasNext()) {
			uint64_t memval=sis->next();
			uint64_t sumval=(memval+val)&getmask(record->getLen());

			if (valset->contains(sumval)) {

				Constraint *loadenc=getStoreLoadSet(record)->getRMWRValueEncoding(this, record, memval);
				Constraint *storeenc=getMemValueEncoding(record, sumval);
				Constraint *notvar=var->negate();
				Constraint *addinputfix=new Constraint(IMPLIES, loadenc, new Constraint(AND, storeenc, notvar));
				ADDCONSTRAINT(addinputfix, "addinputfix");
			}
		}

		delete sis;
	} else {
		ASSERT(depvec->size()==1);
		EPRecord *src=(*depvec)[0];
		IntIterator *it=record->getStoreSet()->iterator();
		IntHashSet *valset=getStoreLoadSet(record)->getValues();

		while(it->hasNext()) {
			uint64_t val=it->next();
			IntIterator *sis=valset->iterator();
			while(sis->hasNext()) {
				uint64_t memval=sis->next();
				uint64_t sum=(memval+val)&getmask(record->getLen());
				if (valset->contains(sum)) {
					Constraint *srcenc=getRetValueEncoding(src, val);
					Constraint *loadenc=getStoreLoadSet(record)->getRMWRValueEncoding(this, record, memval);
					Constraint *storeenc=getMemValueEncoding(record, sum);
					Constraint *notvar=var->negate();
					Constraint *addop=new Constraint(IMPLIES, new Constraint(AND, srcenc, loadenc),
																					 new Constraint(AND, notvar, storeenc));
					ADDCONSTRAINT(addop,"addinputvar");
				}
			}
			delete sis;
		}
		delete it;
	}

	processAddresses(record);
}

/** This function ensures that the value of a store's SAT variables
                matches the store's input value.

                TODO: Could optimize the case where the encodings are the same...
 */

void ConstGen::processStore(EPRecord *record) {
	ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, VC_BASEINDEX);
	if (depvec==NULL) {
		//We have a hard coded value
		uint64_t val=record->getValue(0)->getValue();
		Constraint *storeenc=getMemValueEncoding(record, val);
		ADDCONSTRAINT(storeenc,"storefix");
	} else {
		//We have a value from an input
		ASSERT(depvec->size()==1);
		EPRecord *src=(*depvec)[0];
		IntIterator *it=record->getStoreSet()->iterator();

		while(it->hasNext()) {
			uint64_t val=it->next();
			Constraint *srcenc=getRetValueEncoding(src, val);
			Constraint *storeenc=getMemValueEncoding(record, val);
			Constraint *storevalue=new Constraint(IMPLIES, srcenc, storeenc);
			ADDCONSTRAINT(storevalue,"storevar");
		}
		delete it;
	}
	processAddresses(record);
}

/** **/

void ConstGen::computeYieldCond(EPRecord *record) {
	ExecPoint *yieldep=record->getEP();
	EPRecord *prevyield=NULL;
	ExecPoint *prevyieldep=NULL;

	for(int i=(int)(yieldlist->size()-1);i>=0;i--) {
		EPRecord *tmpyield=(*yieldlist)[i];
		ExecPoint *tmpep=tmpyield->getEP();
		//Do we have a previous yield operation from the same thread
		if (tmpep->get_tid()==yieldep->get_tid() &&
				yieldep->compare(tmpep)==CR_BEFORE) {
			//Yes
			prevyield=tmpyield;
			prevyieldep=prevyield->getEP();
			break;
		}
	}

	yieldlist->push_back(record);

	ModelVector<Constraint *> * novel_branches=new ModelVector<Constraint *>();
	RecordIterator *sri=incompleteexploredbranch->iterator();
	while(sri->hasNext()) {
		EPRecord * unexploredbranch=sri->next();
		ExecPoint * branchep=unexploredbranch->getEP();
		if (branchep->get_tid()!=yieldep->get_tid()) {
			//wrong thread
			continue;
		}

		if (yieldep->compare(branchep)!=CR_BEFORE) {
			//Branch does not occur before yield, so skip
			continue;
		}

		//See if the previous yield already accounts for this branch
		if (prevyield != NULL &&
				prevyieldep->compare(branchep)==CR_BEFORE) {
			//Branch occurs before previous yield, so we can safely skip this branch
			continue;
		}
		//This is a branch that could prevent this yield from being executed
		BranchRecord *br=branchtable->get(unexploredbranch);
		Constraint * novel_branch=br->getNewBranch();
		novel_branches->push_back(novel_branch);
	}

	int num_constraints=((prevyield==NULL)?0:1)+novel_branches->size();
	Constraint *carray[num_constraints];
	int arr_index=0;
	
	if (prevyield!=NULL) {
		carray[arr_index++]=yieldtable->get(prevyield);//get constraint for old yield
	}
	for(uint i=0;i<novel_branches->size();i++) {
		carray[arr_index++]=(*novel_branches)[i];
	}
	
	Constraint *cor=num_constraints!=0?new Constraint(OR, num_constraints, carray):&cfalse;

	Constraint *var=getNewVar();
	//If the variable is true, then we need to have taken some branch
	//ahead of the yield
	Constraint *implies=new Constraint(IMPLIES, var, cor);
	ADDCONSTRAINT(implies, "possiblebranchnoyield");
	yieldtable->put(record, var);
	
	delete novel_branches;
	delete sri;
}


/** Handle yields by just forbidding them via the SAT formula. */

void ConstGen::processYield(EPRecord *record) {
	if (model->params.noexecyields) {
		computeYieldCond(record);
		Constraint * noexecyield=getExecutionConstraint(record)->negate();
		Constraint * branchaway=yieldtable->get(record);
		Constraint * avoidbranch=new Constraint(OR, noexecyield, branchaway);
		ADDCONSTRAINT(avoidbranch, "noexecyield");
	}
}

void ConstGen::processLoopPhi(EPRecord *record) {
	EPRecordIDSet *phiset=record->getPhiLoopTable();
	EPRecordIDIterator *rit=phiset->iterator();

	while(rit->hasNext()) {
		struct RecordIDPair *rip=rit->next();
		EPRecord *input=rip->idrecord;

		IntIterator * it=record->getSet(VC_BASEINDEX)->iterator();
		while(it->hasNext()) {
			uint64_t value=it->next();
			Constraint * inputencoding=getRetValueEncoding(input, value);
			if (inputencoding==NULL)
				continue;
			Constraint * branchconstraint=getExecutionConstraint(rip->record);
			Constraint * outputencoding=getRetValueEncoding(record, value);
			Constraint * phiimplication=new Constraint(IMPLIES, new Constraint(AND, inputencoding, branchconstraint), outputencoding);
			ADDCONSTRAINT(phiimplication,"philoop");
		}
		delete it;
	}
	delete rit;
}

void ConstGen::processPhi(EPRecord *record) {
	ModelVector<EPRecord *> * inputs=execution->getRevDependences(record, VC_BASEINDEX);
	for(uint i=0;i<inputs->size();i++) {
		EPRecord * input=(*inputs)[i];

		IntIterator * it=record->getSet(VC_BASEINDEX)->iterator();
		while(it->hasNext()) {
			uint64_t value=it->next();
			Constraint * inputencoding=getRetValueEncoding(input, value);
			if (inputencoding==NULL)
				continue;
			Constraint * branchconstraint=getExecutionConstraint(input);
			Constraint * outputencoding=getRetValueEncoding(record, value);
			Constraint * phiimplication=new Constraint(IMPLIES, new Constraint(AND, inputencoding, branchconstraint), outputencoding);
			ADDCONSTRAINT(phiimplication,"phi");
		}
		delete it;
	}
}

void ConstGen::processFunction(EPRecord *record) {
	if (record->getLoopPhi()) {
		processLoopPhi(record);
		return;
	} else if (record->getPhi()) {
		processPhi(record);
		return;
	}

	CGoalSet *knownbehaviors=record->completedGoalSet();
	CGoalIterator *cit=knownbehaviors->iterator();
	uint numinputs=record->getNumFuncInputs();
	EPRecord * inputs[numinputs];
	for(uint i=0;i<numinputs;i++) {
		ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, i+VC_BASEINDEX);
		ASSERT(depvec->size()==1);
		inputs[i]=(*depvec)[0];
	}
	while(cit->hasNext()) {
		CGoal *goal=cit->next();
		Constraint *carray[numinputs];
		if (record->isSharedGoals()) {
			bool badvalue=false;
			for(uint i=0;i<numinputs;i++) {
				uint64_t inputval=goal->getValue(i+VC_BASEINDEX);
				if (!record->getSet(i+VC_BASEINDEX)->contains(inputval)) {
					badvalue=true;
					break;
				}
			}
			if (badvalue)
				continue;
		}

		/* Build up constraints for each input */
		for(uint i=0;i<numinputs;i++) {
			uint64_t inputval=goal->getValue(i+VC_BASEINDEX);
			Constraint * inputc=getRetValueEncoding(inputs[i], inputval);
			ASSERT(inputc!=NULL);
			carray[i]=inputc;
		}
		Constraint * outputconstraint=getRetValueEncoding(record, goal->getOutput());
		if (numinputs==0) {
			ADDCONSTRAINT(outputconstraint,"functionimpl");
		} else {
			Constraint * functionimplication=new Constraint(IMPLIES, new Constraint(AND, numinputs, carray), outputconstraint);
			ADDCONSTRAINT(functionimplication,"functionimpl");
		}
	}
	delete cit;

	FunctionRecord *fr=functiontable->get(record);
	Constraint *goal=fr->getNoValueEncoding();
	Constraint *newfunc=new Constraint(AND, goal, getExecutionConstraint(record));
	ADDGOAL(record, newfunc, "newfunc");
}

void ConstGen::processEquals(EPRecord *record) {
	ASSERT (record->getNumFuncInputs() == 2);
	EPRecord * inputs[2];

	for(uint i=0;i<2;i++) {
		ModelVector<EPRecord *> * depvec=execution->getRevDependences(record, i+VC_BASEINDEX);
		if (depvec==NULL) {
			inputs[i]=NULL;
		} else if (depvec->size()==1) {
			inputs[i]=(*depvec)[0];
		}       else ASSERT(false);
	}

	//rely on this being a variable
	Constraint * outputtrue=getRetValueEncoding(record, 1);
	ASSERT(outputtrue->getType()==VAR);

	if (inputs[0]!=NULL && inputs[1]!=NULL &&
			(inputs[0]->getType()==LOAD || inputs[0]->getType()==RMW) &&
			(inputs[1]->getType()==LOAD || inputs[1]->getType()==RMW) &&
			(getStoreLoadSet(inputs[0])==getStoreLoadSet(inputs[1]))) {
		StoreLoadSet * sls=getStoreLoadSet(inputs[0]);
		int numvalvars=sls->getNumValVars();
		Constraint **valvar1=(inputs[0]->getType()==RMW) ? sls->getRMWRValVars(this, inputs[0]) : sls->getValVars(this, inputs[0]);
		Constraint **valvar2=(inputs[1]->getType()==RMW) ? sls->getRMWRValVars(this, inputs[1]) : sls->getValVars(this, inputs[1]);
		//new test

		Constraint *vars[numvalvars];
		for(int i=0;i<numvalvars;i++) {
			vars[i]=getNewVar();
			Constraint *var1=valvar1[i];
			Constraint *var2=valvar2[i];
			Constraint * array[]={var1, var2->negate(), vars[i]->negate()};
			Constraint * array2[]={var2, var1->negate(), vars[i]->negate()};
			Constraint * a=new Constraint(OR, 3, array);
			ADDCONSTRAINT(a, "equala");
			Constraint * a2=new Constraint(OR, 3, array2);
			ADDCONSTRAINT(a2, "equala2");
			Constraint * arrayb[]={var1, var2, vars[i]};
			Constraint * array2b[]={var2->negate(), var1->negate(), vars[i]};
			Constraint * b=new Constraint(OR, 3, arrayb);
			ADDCONSTRAINT(b, "equalb");
			Constraint *b2=new Constraint(OR, 3, array2b);
			ADDCONSTRAINT(b2, "equalb2");
		}
		ADDCONSTRAINT(new Constraint(IMPLIES, new Constraint(AND, numvalvars, vars), outputtrue),"impequal1");

		ADDCONSTRAINT(new Constraint(IMPLIES, outputtrue, new Constraint(AND, numvalvars, vars)), "impequal2");

		/*

		   Constraint * functionimplication=new Constraint(IMPLIES,generateEquivConstraint(numvalvars, valvar1, valvar2), outputtrue);
		   ADDCONSTRAINT(functionimplication,"equalsimplspecial");
		   Constraint * functionimplicationneg=new Constraint(IMPLIES,outputtrue, generateEquivConstraint(numvalvars, valvar1, valvar2));
		   ADDCONSTRAINT(functionimplicationneg,"equalsimplspecialneg");
		 */
		return;
	}

	if (inputs[0]==NULL && inputs[1]==NULL) {
		IntIterator *iit0=record->getSet(VC_BASEINDEX+0)->iterator();
		uint64_t constval=iit0->next();
		delete iit0;
		IntIterator *iit1=record->getSet(VC_BASEINDEX+1)->iterator();
		uint64_t constval2=iit1->next();
		delete iit1;

		if (constval==constval2) {
			ADDCONSTRAINT(outputtrue, "equalsconst");
		} else {
			ADDCONSTRAINT(outputtrue->negate(), "equalsconst");
		}
		return;
	}

	if (inputs[0]==NULL ||
			inputs[1]==NULL) {
		int nullindex=inputs[0]==NULL ? 0 : 1;
		IntIterator *iit=record->getSet(VC_BASEINDEX+nullindex)->iterator();
		uint64_t constval=iit->next();
		delete iit;

		record->getSet(VC_BASEINDEX+nullindex);
		EPRecord *r=inputs[1-nullindex];
		Constraint *l=getRetValueEncoding(r, constval);
		Constraint *functionimplication=new Constraint(IMPLIES, l, outputtrue);
		ADDCONSTRAINT(functionimplication,"equalsimpl");

		Constraint *l2=getRetValueEncoding(r, constval);
		Constraint *functionimplication2=new Constraint(IMPLIES, outputtrue, l2);
		ADDCONSTRAINT(functionimplication2,"equalsimpl");
        return;
	}

	IntIterator *iit=record->getSet(VC_BASEINDEX)->iterator();
	while(iit->hasNext()) {
		uint64_t val1=iit->next();

		IntIterator *iit2=record->getSet(VC_BASEINDEX+1)->iterator();
		while(iit2->hasNext()) {
			uint64_t val2=iit2->next();
			Constraint *l=getRetValueEncoding(inputs[0], val1);
			Constraint *r=getRetValueEncoding(inputs[1], val2);
			Constraint *imp=(val1==val2) ? outputtrue : outputtrue->negate();
			Constraint * functionimplication=new Constraint(IMPLIES, new Constraint(AND, l, r), imp);
			ADDCONSTRAINT(functionimplication,"equalsimpl");
		}
		delete iit2;
	}
	delete iit;
}

#ifdef TSO
void ConstGen::processFence(EPRecord *record) {
	//do we already account for the fence?
	if (isAlwaysExecuted(record))
		return;
	ExecPoint * record_ep=record->getEP();
	thread_id_t tid=record_ep->get_tid();
	uint thread=id_to_int(tid);
	ModelVector<EPRecord *> *tvec=(*threadactions)[thread];
	uint size=tvec->size();

	EPRecord *prevstore=NULL;
	uint i;
	for(i=0;i<size;i++) {
		EPRecord *rec=(*tvec)[i];
		if (rec->getType()==STORE) {
			prevstore=rec;
		}
		if (rec == record) {
			i++;
			break;
		}
	}
	if (prevstore == NULL) {
		return;
	}
	for(;i<size;i++) {
		EPRecord *rec=(*tvec)[i];
		if (rec->getType()==LOAD) {
			Constraint * condition=getExecutionConstraint(record);
			Constraint * storeload=getOrderConstraint(prevstore, rec);
			Constraint * c=new Constraint(IMPLIES, condition, storeload);
			ADDCONSTRAINT(c, "fence");
			return;
		}
	}
}
#endif

/** processRecord performs actions on second traversal of event
		graph. */

void ConstGen::processRecord(EPRecord *record) {
	switch (record->getType()) {
	case FUNCTION:
		processFunction(record);
		break;
	case EQUALS:
		processEquals(record);
		break;
	case LOAD:
		processLoad(record);
		break;
	case STORE:
		processStore(record);
		break;
#ifdef TSO
	case FENCE:
		processFence(record);
		break;
#endif
	case RMW:
#ifdef TSO
		processFence(record);
#endif
		if (record->getOp()==ADD) {
			processAdd(record);
		} else if (record->getOp()==CAS) {
			processCAS(record);
		} else if (record->getOp()==EXC) {
			processEXC(record);
		} else
			ASSERT(0);
		break;
	case YIELD:
		processYield(record);
		break;
	case BRANCHDIR:
		processBranch(record);
		break;
	default:
		break;
	}
}

/** visitRecord performs actions done on first traversal of event
 *	graph. */

void ConstGen::visitRecord(EPRecord *record) {
	switch (record->getType()) {
	case EQUALS:
		recordExecCond(record);
		insertEquals(record);
		break;
	case FUNCTION:
		recordExecCond(record);
		insertFunction(record);
		break;
#ifdef TSO
	case FENCE:
		recordExecCond(record);
		insertAction(record);
		break;
#endif
	case LOAD:
		recordExecCond(record);
		insertAction(record);
		groupMemoryOperations(record);
		break;
	case STORE:
		recordExecCond(record);
		insertAction(record);
		groupMemoryOperations(record);
		break;
	case RMW:
		recordExecCond(record);
		insertAction(record);
		groupMemoryOperations(record);
		break;
	case BRANCHDIR:
		recordExecCond(record);
		insertBranch(record);
		break;
	case LOOPEXIT:
		recordExecCond(record);
		break;
	case NONLOCALTRANS:
		recordExecCond(record);
		insertNonLocal(record);
		break;
	case LABEL:
		insertLabel(record);
		break;
	case YIELD:
		recordExecCond(record);
		break;
	default:
		break;
	}
}

void ConstGen::recordExecCond(EPRecord *record) {
	ExecPoint *eprecord=record->getEP();
	EPValue * branchval=record->getBranch();
	EPRecord * branch=(branchval==NULL) ? NULL : branchval->getRecord();
	ExecPoint *epbranch= (branch==NULL) ? NULL : branch->getEP();
	RecordSet *srs=NULL;
	RecordIterator *sri=nonlocaltrans->iterator();
	while(sri->hasNext()) {
		EPRecord *nonlocal=sri->next();
		ExecPoint *epnl=nonlocal->getEP();
		if (epbranch!=NULL) {
			if (epbranch->compare(epnl)==CR_BEFORE) {
				//branch occurs after non local and thus will subsume condition
				//branch subsumes this condition
				continue;
			}
		}
		if (eprecord->compare(epnl)==CR_BEFORE) {
			//record occurs after non-local, so add it to set
			if (srs==NULL)
				srs=new RecordSet();
			srs->add(nonlocal);
		}
	}
	delete sri;
	if (srs!=NULL)
		execcondtable->put(record, srs);
}

void ConstGen::insertNonLocal(EPRecord *record) {
	nonlocaltrans->add(record);
}

void ConstGen::insertLabel(EPRecord *record) {
	RecordIterator *sri=nonlocaltrans->iterator();
	while(sri->hasNext()) {
		EPRecord *nonlocal=sri->next();
		if (nonlocal->getNextRecord()==record)
			sri->remove();
	}

	delete sri;
}

void ConstGen::traverseRecord(EPRecord *record, bool initial) {
	do {
		if (initial) {
			visitRecord(record);
		} else {
			processRecord(record);
		}
		if (record->getType()==LOOPENTER) {
			if (record->getNextRecord()!=NULL)
				workstack->push_back(record->getNextRecord());
			workstack->push_back(record->getChildRecord());
			return;
		}
		if (record->getChildRecord()!=NULL) {
			workstack->push_back(record->getChildRecord());
		}
		if (record->getType()==NONLOCALTRANS) {
			return;
		}
		if (record->getType()==YIELD && model->params.noexecyields) {
			return;
		}
		if (record->getType()==LOOPEXIT) {
			return;
		}
		if (record->getType()==BRANCHDIR) {
			EPRecord *next=record->getNextRecord();
			if (next != NULL)
				workstack->push_back(next);
			for(uint i=0;i<record->numValues();i++) {
				EPValue *branchdir=record->getValue(i);

				//Could have an empty branch, so make sure the branch actually
				//runs code
				if (branchdir->getFirstRecord()!=NULL)
					workstack->push_back(branchdir->getFirstRecord());
			}
			return;
		} else
			record=record->getNextRecord();
	} while(record!=NULL);
}

unsigned int RecPairHash(RecPair *rp) {
	uintptr_t v=(uintptr_t) rp->epr1;
	uintptr_t v2=(uintptr_t) rp->epr2;
	uintptr_t x=v^v2;
	uintptr_t a=v&v2;
	return (uint)((x>>4)^(a));
}

bool RecPairEquals(RecPair *r1, RecPair *r2) {
	return ((r1->epr1==r2->epr1)&&(r1->epr2==r2->epr2)) ||
				 ((r1->epr1==r2->epr2)&&(r1->epr2==r2->epr1));
}

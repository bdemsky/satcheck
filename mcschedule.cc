/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "mcschedule.h"
#include "mcexecution.h"

WaitPair::WaitPair(ExecPoint *stoppoint, ExecPoint *waitpoint) :
	stop_point(stoppoint),
	wait_point(waitpoint) {
}

WaitPair::~WaitPair() {
}

ExecPoint * WaitPair::getStop() {
	return stop_point;
}

ExecPoint * WaitPair::getWait() {
	return wait_point;
}

MCScheduler::MCScheduler(MCExecution *e) :
	waitsetlen(4),
	execution(e),
	waitset(new ModelVector<ModelVector<WaitPair *> *>()),
	waitsetindex((unsigned int *)model_calloc(sizeof(unsigned int)*waitsetlen, 1)),
	newbehavior(false)
{
#ifdef TSO
	storesetindex=(unsigned int *)model_calloc(sizeof(unsigned int)*waitsetlen, 1);
	storeset=new ModelVector<ModelVector<WaitPair *> *>();
#endif
}

MCScheduler::~MCScheduler() {
}

void MCScheduler::setNewFlag() {
	newbehavior=true;
}

void MCScheduler::addWaitPair(EPRecord *stoprec, EPRecord *waitrec) {
	ExecPoint *stoppoint=stoprec->getEP();
	ExecPoint *waitpoint=waitrec->getEP();

	uint tid=id_to_int(stoppoint->get_tid());
	if (waitset->size()<=tid) {
		uint oldsize=waitset->size();
		waitset->resize(tid+1);
		for(uint i=oldsize;i<=tid;i++) {
			(*waitset)[i]=new ModelVector<WaitPair *>();
		}
#ifdef TSO
		storeset->resize(tid+1);
		for(uint i=oldsize;i<=tid;i++) {
			(*storeset)[i]=new ModelVector<WaitPair *>();
		}
#endif
	}
	if (tid>=waitsetlen) {
		waitsetindex=(uint *) model_realloc(waitsetindex, sizeof(uint)*(tid << 1));
		for(uint i=waitsetlen;i<(tid<<1);i++)
			waitsetindex[i]=0;
#ifdef TSO
		storesetindex=(uint *) model_realloc(storesetindex, sizeof(uint)*(tid << 1));
		for(uint i=waitsetlen;i<(tid<<1);i++)
			storesetindex[i]=0;
#endif
		waitsetlen=tid<<1;
	}
#ifdef TSO
	if (stoprec->getType()==STORE)
		(*storeset)[tid]->push_back(new WaitPair(stoppoint, waitpoint));
	else
		(*waitset)[tid]->push_back(new WaitPair(stoppoint, waitpoint));
#else
	(*waitset)[tid]->push_back(new WaitPair(stoppoint, waitpoint));
#endif
}

void MCScheduler::reset() {
	for(unsigned int i=0;i<waitset->size();i++) {
		ModelVector<WaitPair*> * v=(*waitset)[i];
		for(unsigned int j=0;j<v->size();j++) {
			WaitPair *wp=(*v)[j];
			delete wp;
		}
		v->clear();
	}
	for(unsigned int i=0;i<waitsetlen;i++)
		waitsetindex[i]=0;
#ifdef TSO
	for(unsigned int i=0;i<storeset->size();i++) {
		ModelVector<WaitPair*> * v=(*storeset)[i];
		for(unsigned int j=0;j<v->size();j++) {
			WaitPair *wp=(*v)[j];
			delete wp;
		}
		v->clear();
	}
	for(unsigned int i=0;i<waitsetlen;i++)
		storesetindex[i]=0;
#endif
	newbehavior=false;
}



 void MCScheduler::check_preempt() {
	Thread *t=execution->get_current_thread();
#ifdef TSO
 restart_search:
	//start at the next thread
	unsigned int tid=id_to_int(t->get_id());
	bool storebuffer=true;
	for(unsigned int i=0;i<(2*execution->get_num_threads());i++) {
		if (storebuffer) {
			//Don't try to do a store from an empty store buffer
			if (!execution->isEmptyStoreBuffer(int_to_id(tid))&&
					check_store_buffer(tid)) {
				execution->doStore(int_to_id(tid));
				goto restart_search;
			}
			tid=(tid+1)%execution->get_num_threads();
			storebuffer=false;
		} else {
			//don't try to schedule finished threads
			if (!execution->get_thread(int_to_id(tid))->is_complete() &&
					check_thread(tid)) {
				break;
			}
			storebuffer=true;
		}
	}
#else
	//start at the next thread
	unsigned int tid=(id_to_int(t->get_id())+1)%execution->get_num_threads();
	
	for(unsigned int i=0;i<execution->get_num_threads();i++,tid=(tid+1)%execution->get_num_threads()) {
		//don't try to schedule finished threads
		if (execution->get_thread(int_to_id(tid))->is_complete())
			continue;
		if (check_thread(tid)) {
			break;
		}
	}
#endif
	
	Thread *next_thread=execution->get_thread(int_to_id(tid));
	if (next_thread->is_complete())
		next_thread=NULL;

	swap_threads(t,next_thread);
}

#ifdef TSO
bool MCScheduler::check_store_buffer(unsigned int tid) {
	ExecPoint *current=execution->getStoreBuffer(tid)->getEP();
	return checkSet(tid, storeset, storesetindex, current);
}
#endif

bool MCScheduler::check_thread(unsigned int tid) {
	ExecPoint *current=execution->get_execpoint(tid);
	return checkSet(tid, waitset, waitsetindex, current);
}


bool MCScheduler::checkSet(unsigned int tid, ModelVector<ModelVector<WaitPair* > * > *set, unsigned int *setindex, ExecPoint *current) {
	if (tid >= set->size())
		return true;
	ModelVector<WaitPair *> * wps=&(*((*set)[tid]));

	if (!newbehavior) {
		for(;setindex[tid]<wps->size();setindex[tid]++) {
			WaitPair *nextwp=(*wps)[setindex[tid]];
			ExecPoint *stoppoint=nextwp->getStop();
			CompareResult compare=stoppoint->compare(current);
			if (compare==CR_EQUALS) {
				//hit stop point
				ExecPoint *waitpoint=nextwp->getWait();		
				thread_id_t waittid=waitpoint->get_tid();
				ExecPoint *waitthread=execution->get_execpoint(waittid);
				CompareResult comparewt=waitthread->compare(waitpoint);
				//we've resolved this wait, keep going
				if (comparewt==CR_BEFORE||comparewt==CR_INCOMPARABLE) {
#ifdef TSO
					EPRecord *waitrec=execution->getRecord(waitpoint);
					if (waitrec->getType()==STORE) {
						EPValue *first_store=execution->getStoreBuffer(waittid);
						if (first_store==NULL)
							continue;
						ExecPoint *storebuffer_ep=first_store->getEP();
						CompareResult sb_comparewt=storebuffer_ep->compare(waitpoint);
						if (sb_comparewt==CR_BEFORE||sb_comparewt==CR_INCOMPARABLE)
							continue;
						//need to wait to commit store out of buffer
						return false;
					} else
						continue;
#else
					continue;
#endif
				}
#ifdef TSO
				//wait for store buffer to empty
				if (!execution->isEmptyStoreBuffer(waittid)) {
					return false;
				}
#endif
				//don't wait on a completed thread if store buffer is empty
				if (execution->get_thread(id_to_int(waittid))->is_complete())
					continue;
				//Need to wait for another thread to take a step
				return false;
			} else if (compare==CR_BEFORE) {
				//we haven't reached the context switch point
				return true;
			} else {
				return true;
				//oops..missed the point...
				//this means we are past the point of our model's validity...
			}
		}
	}
	return true;
}

/** Swap context from thread from to thread to.  If a thread is NULL,
 *  then we switch to the system context. */

void MCScheduler::swap_threads(Thread * from, Thread *to) {
	ucontext_t * fromcontext=(from==NULL) ? get_system_context() : from->get_context();
	ucontext_t * tocontext=(to==NULL) ? get_system_context() : to->get_context();
	execution->set_current_thread(to);
	model_swapcontext(fromcontext, tocontext);
}

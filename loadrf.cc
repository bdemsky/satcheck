/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "loadrf.h"
#include "constgen.h"
#include "mcutil.h"
#include "constraint.h"
#include "storeloadset.h"
#include "eprecord.h"
#include "execpoint.h"

LoadRF::LoadRF(EPRecord *_load, ConstGen *cg) : load(_load) {
	RecordSet *mrfSet=cg->getMayReadFromSet(load);
	uint numstores=mrfSet->getSize();
	numvars=NUMBITS(numstores-1);
	vars=(Constraint **)model_malloc(numvars*sizeof(Constraint *));
	cg->getArrayNewVars(numvars, vars);
}

LoadRF::~LoadRF() {
	model_free(vars);
}

void LoadRF::genConstraints(ConstGen *cg) {
	uint storeindex=0;
	RecordSet *mrfSet=cg->getMayReadFromSet(load);
	StoreLoadSet * sls=cg->getStoreLoadSet(load);
	uint numvalvars=sls->getNumValVars();
	uint numaddrvars=sls->getNumAddrVars();
	Constraint ** loadvalvars=(load->getType()==RMW) ? sls->getRMWRValVars(cg, load) : sls->getValVars(cg, load);
	Constraint ** loadaddrvars=sls->getAddrVars(cg, load);

	RecordIterator *sri=mrfSet->iterator();
	while(sri->hasNext()) {
		EPRecord *store=sri->next();
		Constraint ** storevalvars=sls->getValVars(cg, store);


		Constraint *rfconst=(numvars==0) ? &ctrue : generateConstraint(numvars, vars, storeindex);
		//if we read from a store, it should happen before us
#ifdef TSO
		Constraint *storebeforeload;
		if (store->getEP()->get_tid()==load->getEP()->get_tid()) {
			if (store->getEP()->compare(load->getEP())==CR_AFTER) {
				storebeforeload=&ctrue;
			} else
				storebeforeload=&cfalse;
		} else
			storebeforeload=cg->getOrderConstraint(store, load);
#else
		Constraint * storebeforeload=cg->getOrderConstraint(store, load);
#endif
		//if we read from a store, we should have the same value
		Constraint *storevalmatch=generateEquivConstraint(numvalvars, loadvalvars, storevalvars);
		//if we read from a store, it must have been executed
		Constraint *storeexecuted=cg->getExecutionConstraint(store);



		//if we read from a store, we should have the same address
		Constraint ** storeaddrvars=sls->getAddrVars(cg, store);
		Constraint * storeaddressmatch=generateEquivConstraint(numaddrvars, loadaddrvars, storeaddrvars);
		Constraint *array[] = {storebeforeload, storevalmatch, storeexecuted, storeaddressmatch};


		ADDCONSTRAINT2(cg, new Constraint(IMPLIES, rfconst, new Constraint(AND, 4, array)), "storeaddressmatch");


		//Also need to make sure that we don't have conflicting stores
		//ordered between the load and store

		RecordSet *conflictSet=cg->computeConflictSet(store, load, mrfSet);
		RecordIterator *sit=conflictSet!=NULL ? conflictSet->iterator() : mrfSet->iterator();

		while(sit->hasNext()) {
			EPRecord *conflictstore=sit->next();
			Constraint ** confstoreaddrvars=sls->getAddrVars(cg, conflictstore);
			Constraint * storebeforeconflict=cg->getOrderConstraint(store, conflictstore);
			if (storebeforeconflict->isFalse())
				continue;
#ifdef TSO
			//Have to cover the same thread case
			Constraint *conflictbeforeload;
			if (conflictstore->getEP()->get_tid()==load->getEP()->get_tid()) {
				if (conflictstore->getEP()->compare(load->getEP())==CR_AFTER)
					conflictbeforeload=&ctrue;
				else
					conflictbeforeload=&cfalse;
			} else {
				conflictbeforeload=cg->getOrderConstraint(conflictstore, load);
			}
			if (conflictbeforeload->isFalse()) {
				storebeforeconflict->freerec();
				continue;
			}
#else
			Constraint * conflictbeforeload=cg->getOrderConstraint(conflictstore, load);
			if (conflictbeforeload->isFalse()) {
				storebeforeconflict->freerec();
				continue;
			}
#endif
			rfconst=generateConstraint(numvars, vars, storeindex);
			Constraint *array[]={storebeforeconflict,
													 conflictbeforeload,
													 rfconst,
													 cg->getExecutionConstraint(conflictstore)};
			Constraint *confstore=new Constraint(IMPLIES,
																					 new Constraint(AND, 4, array),
																					 generateEquivConstraint(numaddrvars, loadaddrvars, confstoreaddrvars)->negate());

			ADDCONSTRAINT2(cg, confstore, "confstore");
		}
		delete sit;
		if (conflictSet != NULL)
			delete conflictSet;

		storeindex++;
	}
	delete sri;

	Constraint *mustrf=generateLTConstraint(cg, numvars, vars, storeindex);
	Constraint *validrf=new Constraint(IMPLIES, cg->getExecutionConstraint(load), mustrf);
	ADDCONSTRAINT2(cg, validrf, "validrf");
}

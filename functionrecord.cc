/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "functionrecord.h"
#include "constraint.h"
#include "constgen.h"
#include "eprecord.h"

FunctionRecord::FunctionRecord(ConstGen *cg, EPRecord *func) : function(func) {
	if (function->getPhi()) {
		uint numvals=function->getSet(VC_BASEINDEX)->getSize();
		numvars=NUMBITS(numvals-1);
		vars=(Constraint **)model_malloc(numvars*sizeof(Constraint *));
		cg->getArrayNewVars(numvars, vars);
	} else {
		uint numvals=function->getSet(VC_FUNCOUTINDEX)->getSize();
		numvals++;//allow for new combinations in sat formulas
		numvars=NUMBITS(numvals-1);
		vars=(Constraint **)model_malloc(numvars*sizeof(Constraint *));
		cg->getArrayNewVars(numvars, vars);
	}
}

FunctionRecord::~FunctionRecord() {
	model_free(vars);
}

Constraint * FunctionRecord::getValueEncoding(uint64_t val) {
	int index=-1;
	if (function->getPhi()) {
		IntIterator *it=function->getSet(VC_BASEINDEX)->iterator();
		int count=0;
		while(it->hasNext()) {
			if (it->next()==val) {
				index=count;
				break;
			}
			count++;
		}
		delete it;
	} else {
		IntIterator *it=function->getSet(VC_FUNCOUTINDEX)->iterator();
		int count=0;
		while(it->hasNext()) {
			if (it->next()==val) {
				index=count;
				break;
			}
			count++;
		}
		delete it;
	}
	if (index==-1)
		return NULL;
	return generateConstraint(numvars, vars, (uint)index);
}

Constraint * FunctionRecord::getNoValueEncoding() {
	return new Constraint(AND, numvars, vars);
}

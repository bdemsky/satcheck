/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "branchrecord.h"
#include "mymemory.h"
#include "constraint.h"
#include "eprecord.h"

BranchRecord::BranchRecord(EPRecord *r, uint nvars, Constraint **cvars, bool isalwaysexecuted) :
	branch(r),
	hasNext(r->getNextRecord()!=NULL),
	numvars(nvars),
	numdirections(r->numValues()),
	alwaysexecuted(isalwaysexecuted),
	vars((Constraint **)model_malloc(sizeof(Constraint *)*nvars)) {
	memcpy(vars, cvars, sizeof(Constraint *)*nvars);
}

BranchRecord::~BranchRecord() {
	model_free(vars);
}

Constraint * BranchRecord::getAnyBranch() {
	if (alwaysexecuted)
		return &ctrue;
	else
		return new Constraint(OR, numvars, vars);
}

Constraint * BranchRecord::getNewBranch() {
	return new Constraint(AND, numvars, vars);
}

Constraint * BranchRecord::getBranch(EPValue *val) {
	int index=branch->getIndex(val);
	ASSERT(index>=0);
	if (!alwaysexecuted)
		index++;
	return generateConstraint(numvars, vars, index);
}

int BranchRecord::getBranchValue(bool * array) {
	uint value=0;
	for(int j=numvars-1;j>=0;j--) {
		value=value<<1;
		if (array[vars[j]->getVar()])
			value|=1;
	}
	if (!alwaysexecuted)
		value--;
	
	return value;
}

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "valuerecord.h"
#include "mymemory.h"
#include "constraint.h"
#include "constgen.h"
#include "mcutil.h"

ValueRecord::ValueRecord(IntHashSet *_set) : set(_set)
{
	uint numvalue=set->getSize();
	numvars=NUMBITS(numvalue-1);
}

ValueRecord::~ValueRecord() {
}

uint64_t ValueRecord::getValue(Constraint **vars, bool *satsolution) {
	uint value=0;
	for(int j=numvars-1;j>=0;j--) {
		value=value<<1;
		if (satsolution[vars[j]->getVar()])
			value|=1;
	}
	uint i=0;
	IntIterator * it=set->iterator();
	while(it->hasNext()) {
		uint64_t val=it->next();
		if (numvars==0) {
			delete it;
			return val;
		}
		if (i==value) {
			delete it;
			return val;
		}
		i++;
	}
	delete it;
	return 111111111;
}

Constraint * ValueRecord::getValueEncoding(Constraint **vars, uint64_t value) {
	uint i=0;
	IntIterator * it=set->iterator();
	while(it->hasNext()) {
		if (it->next()==value) {
			delete it;
			if (numvars==0) {
				return &ctrue;
			}
			return generateConstraint(numvars, vars, i);
		}
		i++;
	}
	delete it;


	return NULL;
}

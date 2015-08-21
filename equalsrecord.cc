/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "equalsrecord.h"
#include "constraint.h"
#include "constgen.h"
#include "eprecord.h"

EqualsRecord::EqualsRecord(ConstGen *cg, EPRecord *eq) : equals(eq) {
	vars=cg->getNewVar();
}

EqualsRecord::~EqualsRecord() {
}

Constraint * EqualsRecord::getValueEncoding(uint64_t val) {
	if (val)
		return vars;
	else
		return vars->negate();
}


/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef EQUALSRECORD_H
#define EQUALSRECORD_H
#include "classlist.h"

class EqualsRecord {
public:
	EqualsRecord(ConstGen *cg, EPRecord *func);
	~EqualsRecord();
	Constraint * getValueEncoding(uint64_t val);
	EPRecord *getRecord() {return equals;}

	MEMALLOC;
private:
	EPRecord *equals;
	Constraint *vars;
};
#endif

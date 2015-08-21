/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef VALUERECORD_H
#define VALUERECORD_H
#include "classlist.h"

class ValueRecord {
 public:
	ValueRecord(IntHashSet *set);
  ~ValueRecord();
  Constraint * getValueEncoding(Constraint **vars, uint64_t value);
	uint64_t getValue(Constraint **vars, bool *satsolution);
	uint getNumVars() {return numvars;}
	MEMALLOC;
 private:
	IntHashSet *set;
	uint numvars;
};
#endif

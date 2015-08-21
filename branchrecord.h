/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef BRANCHRECORD_H
#define BRANCHRECORD_H
#include "classlist.h"

class BranchRecord {
 public:
	BranchRecord(EPRecord *record, uint numvars, Constraint **vars, bool isalwaysexecuted);
	~BranchRecord();
	Constraint * getAnyBranch();
	Constraint * getBranch(EPValue *val);
	Constraint * getNewBranch();
	int getBranchValue(bool *array);
	int numDirections() {return numdirections;}
	bool hasNextRecord() {return hasNext;}
	MEMALLOC;
 private:
  EPRecord *branch;
	bool hasNext;
  uint numvars;
	uint numdirections;
	bool alwaysexecuted;
  Constraint **vars;
};
#endif

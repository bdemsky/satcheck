/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef FUNCTIONRECORD_H
#define FUNCTIONRECORD_H
#include "classlist.h"

class FunctionRecord {
 public:
	FunctionRecord(ConstGen *cg, EPRecord *func);
	~FunctionRecord();
	Constraint * getValueEncoding(uint64_t val);
	Constraint * getNoValueEncoding();

	MEMALLOC;
 private:
	EPRecord *function;
  Constraint **vars;
	uint numvars;
};
#endif

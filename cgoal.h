/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef C_GOAL_H
#define C_GOAL_H
#include "classlist.h"
#include "mymemory.h"
#include "hashset.h"
#include <stdint.h>

class CGoal {
public:
	CGoal(unsigned int num, uint64_t *vals);
	~CGoal();
	unsigned int getNum() {return num;}
	uint64_t getValue(unsigned int i) {return valarray[i];}
	void setOutput(uint64_t _output) { outputvalue=_output;}
	uint64_t getOutput() {return outputvalue;}
	void print();

	MEMALLOC;
private:
	uint64_t * valarray;
	uint64_t outputvalue;
	unsigned int num;
	unsigned int hash;

	friend bool CGoalEquals(CGoal *cg1, CGoal *cg2);
	friend unsigned int CGoalHash(CGoal *cg);
};

bool CGoalEquals(CGoal *cg1, CGoal *cg2);
unsigned int CGoalHash(CGoal *cg);

typedef HashSet<CGoal *, uintptr_t, 0, model_malloc, model_calloc, model_free, CGoalHash, CGoalEquals> CGoalSet;
typedef HSIterator<CGoal *, uintptr_t, 0, model_malloc, model_calloc, model_free, CGoalHash, CGoalEquals> CGoalIterator;

#endif

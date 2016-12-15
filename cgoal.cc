/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "cgoal.h"
#include "execpoint.h"
#include "eprecord.h"

CGoal::CGoal(unsigned int _num, uint64_t *vals) :
	outputvalue(0),
	num(_num),
	hash(_num)
{
	valarray=(uint64_t *)model_malloc(sizeof(uint64_t)*num);
	for(unsigned int i=0;i<num;i++) {
		hash^=(valarray[i]=vals[i]);
	}
}

CGoal::~CGoal() {
	if (valarray)
		model_free(valarray);
}

void CGoal::print() {
	model_print("goal: ");
	model_print("(");
	for(uint i=0;i<num;i++) {
		model_print("%llu",valarray[i]);
		if ((i+1)!=num)
			model_print(", ");
	}
	model_print(")");
}

bool CGoalEquals(CGoal *cg1, CGoal *cg2) {
	if (cg1==NULL) {
		return cg2==NULL;
	}
	if (cg1->hash!=cg2->hash||
			cg1->num!=cg2->num)
		return false;
	if (cg1->valarray) {
		for(unsigned int i=0;i<cg1->num;i++) {
			if (cg1->valarray[i]!=cg2->valarray[i])
				return false;
		}
	}

	return true;
}

unsigned int CGoalHash(CGoal *cg) {
	return cg->hash;
}


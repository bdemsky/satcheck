/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "change.h"

MCChange::MCChange(EPRecord * _record, uint64_t _val, unsigned int _index) :
	record(_record),
	val(_val),
	index(_index)
{
	ASSERT(record->getType()!=BRANCHDIR);
}

MCChange::~MCChange() {
}

void MCChange::print() {
	record->print();
	model_print("(");
	model_print("index=%u, val=%lu)\n", index, val);
}

bool MCChangeEquals(MCChange *mcc1, MCChange *mcc2) {
	if (mcc1==NULL)
		return mcc1==mcc2;
	return (mcc1->record==mcc2->record) && (mcc1->val==mcc2->val) && (mcc1->index == mcc2->index);
}

unsigned int MCChangeHash(MCChange *mcc) {
	return ((unsigned int)((uintptr_t)mcc->record))^((unsigned int)mcc->val)^mcc->index;
}


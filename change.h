/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef CHANGE_H
#define CHANGE_H
#include "classlist.h"
#include "mymemory.h"
#include <stdint.h>
#include "eprecord.h"

class MCChange {
public:
	MCChange(EPRecord *record, uint64_t _val, unsigned int _index);
	~MCChange();
	EPRecord *getRecord() {return record;}
	int getIndex() {return index;}
	uint64_t getValue() {return val;}
	bool isRMW() {return record->getType()==RMW;}
	bool isFunction() {return record->getType()==FUNCTION;}
	bool isEquals() {return record->getType()==EQUALS;}
	bool isLoad() {return record->getType()==LOAD;}
	bool isStore() {return record->getType()==STORE;}
	void print();
	MEMALLOC;

private:
	EPRecord *record;
	uint64_t val;
	unsigned int index;
	friend bool MCChangeEquals(MCChange *mcc1, MCChange *mcc2);
	friend unsigned int MCChangeHash(MCChange *mcc);
};

bool MCChangeEquals(MCChange *mcc1, MCChange *mcc2);
unsigned int MCChangeHash(MCChange *mcc);
#endif

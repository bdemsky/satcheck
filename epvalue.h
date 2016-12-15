/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef EPVALUE_H
#define EPVALUE_H
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"
#include "mcutil.h"

#define VC_ADDRINDEX 0
#define VC_RFINDEX 1
#define VC_BASEINDEX 2
#define VC_OLDVALCASINDEX 3
#define VC_RMWOUTINDEX 4
#define VC_VALOUTINDEX 5
#define VC_FUNCOUTINDEX 1

class EPValue {
public:
	EPValue(ExecPoint *, EPRecord *, const void *addr, uint64_t value, int len);
	~EPValue();
	uint64_t getValue() {return value;}
	ExecPoint * getEP() {return execpoint;}
	const void *getAddr() {return addr;}
	int getLen() {return len;}
	EPRecord * getFirstRecord() {return firstrecord;}
	EPRecord *firstrecord;
	EPRecord *lastrecord;
	EPRecord * getRecord() {return record;}

	MEMALLOC;
private:
	ExecPoint * execpoint;
	EPRecord *record;
	const void *addr;
	uint64_t value;
	int len;
	friend bool EPValueEquals(EPValue * ep1, EPValue *ep2);
	friend unsigned int EPValueHash(EPValue *ep);
};

bool EPValueEquals(EPValue * ep1, EPValue *ep2);
unsigned int EPValueHash(EPValue *ep);
#endif

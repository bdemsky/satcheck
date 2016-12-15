/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef STORELOADSET_H
#define STORELOADSET_H
#include "classlist.h"
#include "stl-model.h"

class StoreLoadSet {
public:
	StoreLoadSet();
	~StoreLoadSet();
	void add(EPRecord *op);
	RecordIterator * iterator() {return storeloadset.iterator();}
	Constraint * getAddressEncoding(ConstGen *cg, EPRecord *op, const void * addr);
	Constraint * getValueEncoding(ConstGen *cg, EPRecord *op, uint64_t val);
	Constraint * getRMWRValueEncoding(ConstGen *cg, EPRecord *op, uint64_t val);

	const void * getAddressEncoding(ConstGen *cg, EPRecord *op, bool *);
	uint64_t getValueEncoding(ConstGen *cg, EPRecord *op,  bool *);
	uint64_t getRMWRValueEncoding(ConstGen *cg, EPRecord *op, bool *);
	uint getNumAddrVars();
	uint getNumValVars();
	Constraint ** getAddrVars(ConstGen *cg, EPRecord * op);
	Constraint ** getValVars(ConstGen *cg, EPRecord * op);
	Constraint ** getRMWRValVars(ConstGen *cg, EPRecord * op);
	IntHashSet * getValues() {return &values;}
	bool removeAddress(const void *addr) {addresses.remove((uint64_t)addr);return addresses.isEmpty();}

	MEMALLOC;
private:
	void genEncoding();
	RecordSet storeloadset;
	IntHashSet addresses;
	IntHashSet values;
	VarTable addrvars;
	VarTable valvars;
	VarTable valrmwrvars;
	ValueRecord *addrencodings;
	ValueRecord *valencodings;
};
#endif

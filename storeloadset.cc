/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "storeloadset.h"
#include "eprecord.h"
#include "valuerecord.h"
#include "constgen.h"

StoreLoadSet::StoreLoadSet() :
	addrencodings(NULL),
	valencodings(NULL) {
}

StoreLoadSet::~StoreLoadSet() {
	if (addrencodings!=NULL) {
		delete addrencodings;
		delete valencodings;
	}
	addrvars.resetandfree();
	valvars.resetandfree();
	valrmwrvars.resetandfree();
}

void StoreLoadSet::add(EPRecord *op) {
	storeloadset.add(op);
	IntIterator * it=op->getSet(VC_ADDRINDEX)->iterator();
	while(it->hasNext())
		addresses.add(it->next());
	delete it;

	if (op->getType()==STORE ||
			op->getType()==RMW) {
		it=op->getStoreSet()->iterator();
		while(it->hasNext())
			values.add(it->next());
		delete it;
	}
}

void StoreLoadSet::genEncoding() {
	addrencodings=new ValueRecord(&addresses);
	valencodings=new ValueRecord(&values);
}

const void * StoreLoadSet::getAddressEncoding(ConstGen *cg, EPRecord *op, bool *sat) {
	Constraint **vars=getAddrVars(cg, op);
	return (const void *) addrencodings->getValue(vars, sat);
}

uint64_t StoreLoadSet::getValueEncoding(ConstGen *cg, EPRecord *op,  bool *sat) {
	Constraint **vars=getValVars(cg, op);
	return valencodings->getValue(vars, sat);
}

uint64_t StoreLoadSet::getRMWRValueEncoding(ConstGen *cg, EPRecord *op, bool *sat) {
	Constraint **vars=getRMWRValVars(cg, op);
	return valencodings->getValue(vars, sat);
}

Constraint * StoreLoadSet::getAddressEncoding(ConstGen *cg, EPRecord *op, const void * addr) {
	if (addrencodings==NULL)
		genEncoding();
	Constraint **vars=getAddrVars(cg, op);
	return addrencodings->getValueEncoding(vars, (uint64_t) addr);
}

Constraint * StoreLoadSet::getValueEncoding(ConstGen *cg, EPRecord *op, uint64_t val) {
	if (valencodings==NULL)
		genEncoding();
	Constraint **vars=getValVars(cg, op);
	return valencodings->getValueEncoding(vars, val);
}

Constraint * StoreLoadSet::getRMWRValueEncoding(ConstGen *cg, EPRecord *op, uint64_t val) {
	if (valencodings==NULL)
		genEncoding();
	Constraint **vars=getRMWRValVars(cg, op);
	return valencodings->getValueEncoding(vars, val);
}

Constraint ** StoreLoadSet::getAddrVars(ConstGen *cg, EPRecord * op) {
	Constraint **vars=addrvars.get(op);
	if (vars==NULL) {
		int numvars=addrencodings->getNumVars();
		vars=(Constraint **)model_malloc(numvars*sizeof(Constraint *));
		cg->getArrayNewVars(numvars, vars);
		addrvars.put(op, vars);
	}
	return vars;
}

Constraint ** StoreLoadSet::getValVars(ConstGen *cg, EPRecord * op) {
	Constraint **vars=valvars.get(op);
	if (vars==NULL) {
		int numvars=valencodings->getNumVars();
		vars=(Constraint **)model_malloc(numvars*sizeof(Constraint *));
		cg->getArrayNewVars(numvars, vars);
		valvars.put(op, vars);
	}
	return vars;
}

Constraint ** StoreLoadSet::getRMWRValVars(ConstGen *cg, EPRecord * op) {
	Constraint **vars=valrmwrvars.get(op);
	if (vars==NULL) {
		int numvars=valencodings->getNumVars();
		vars=(Constraint **)model_malloc(numvars*sizeof(Constraint *));
		cg->getArrayNewVars(numvars, vars);
		valrmwrvars.put(op, vars);
	}
	return vars;
}

uint StoreLoadSet::getNumValVars() {
	if (valencodings==NULL)
		genEncoding();
	return valencodings->getNumVars();
}

uint StoreLoadSet::getNumAddrVars() {
	if (addrencodings==NULL)
		genEncoding();
	return addrencodings->getNumVars();
}

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "epvalue.h"
#include "execpoint.h"
#include "mcutil.h"

EPValue::EPValue(ExecPoint * ep, EPRecord * epr, const void * _addr, uint64_t ivalue, int ilen) :
	firstrecord(NULL),
	lastrecord(NULL),
	execpoint(ep),
	record(epr),
	addr(_addr),
	value(keepbytes(ivalue, ilen)),
	len(ilen)
{
}

EPValue::~EPValue() {
}

bool EPValueEquals(EPValue * ep1, EPValue *ep2) {
	if (ep1 == NULL)
		return ep2 == NULL;

	if (ep1->addr != ep2->addr || ep1->len!=ep2->len || ep1->value!=ep2->value)
		return false;
	return ExecPointEquals(ep1->execpoint, ep2->execpoint);
}

unsigned int EPValueHash(EPValue *ep) {
	return ExecPointHash(ep->execpoint) ^ ep->len ^ ep->value ^ ((intptr_t)ep->addr);
}

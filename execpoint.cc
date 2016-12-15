/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "execpoint.h"
#include "mymemory.h"
#include <string.h>
#include "mcutil.h"
#include "common.h"
#include <stdlib.h>

ExecPoint::ExecPoint(int _length, thread_id_t thread_id) :
	length(_length<<1),
	size(0),
	pairarray((execcount_t *)model_malloc(sizeof(execcount_t)*length)),
	hashvalue(id_to_int(thread_id)),
	tid(thread_id)
{
}

ExecPoint::ExecPoint(ExecPoint *e) :
	length(e->size),
	size(e->size),
	pairarray((execcount_t *)model_malloc(sizeof(execcount_t)*length)),
	hashvalue(e->hashvalue),
	tid(e->tid)
{
	memcpy(pairarray, e->pairarray, sizeof(execcount_t)*length);
}

ExecPoint::~ExecPoint() {
	model_free(pairarray);
}

void ExecPoint::reset() {
	size=0;
	hashvalue=id_to_int(tid);
}

/** Return CR_AFTER means that ep occurs after this.
                Return CR_BEFORE means tha te occurs before this.
                Return CR_EQUALS means that they are the same point.
                Return CR_INCOMPARABLE means that they are not comparable.
 */

CompareResult ExecPoint::compare(const ExecPoint * ep) const {
	if (this==ep)
		return CR_EQUALS;
	unsigned int minsize=(size<ep->size) ? size : ep->size;
	for(unsigned int i=0;i<minsize;i+=2) {
		ASSERT(pairarray[i]==ep->pairarray[i]);
		if (pairarray[i+1]!=ep->pairarray[i+1]) {
			//we have a difference
			if (pairarray[i]==EP_BRANCH)
				return CR_INCOMPARABLE;
			else if (pairarray[i+1]<ep->pairarray[i+1])
				return CR_AFTER;
			else
				return CR_BEFORE;
		}
	}
	if (size < ep->size)
		return CR_AFTER;
	else if (size > ep->size)
		return CR_BEFORE;
	else
		return CR_EQUALS;
}

void ExecPoint::pop() {
	hashvalue = rotate_right((hashvalue ^ pairarray[size-1]), 3);
	size-=2;
}

bool ExecPoint::directInLoop() {
	return ((ExecPointType)pairarray[size-4])==EP_LOOP;
}

bool ExecPoint::inLoop() {
	int index;
	for(index=size-2;index>0;index-=2) {
		if ((ExecPointType)pairarray[index]==EP_LOOP)
			return true;
	}
	return false;
}

ExecPointType ExecPoint::getType() {
	return (ExecPointType)pairarray[size-2];
}

void ExecPoint::push(ExecPointType type, execcount_t value) {
	if (size==length) {
		length=length<<1;
		execcount_t *tmp=(execcount_t *)model_malloc(sizeof(execcount_t)*length);
		memcpy(tmp, pairarray, sizeof(execcount_t)*size);
		model_free(pairarray);
		pairarray=tmp;
	}
	hashvalue = rotate_left(hashvalue, 3) ^ value;
	pairarray[size++]=type;
	pairarray[size++]=value;
}

void ExecPoint::incrementTop() {
	execcount_t current=pairarray[size-1];
	hashvalue = hashvalue ^ current ^ (current+1);
	pairarray[size-1]=current+1;
}

unsigned int ExecPointHash(ExecPoint *e) {
	return e->hashvalue;
}

bool ExecPointEquals(ExecPoint *e1, ExecPoint * e2) {
	if (e1 == NULL)
		return e2==NULL;
	if (e1->tid != e2->tid || e1->size != e2->size || e1->hashvalue != e2->hashvalue)
		return false;
	for(unsigned int i=0;i<e1->size;i++) {
		if (e1->pairarray[i]!=e2->pairarray[i])
			return false;
	}
	return true;
}

void ExecPoint::print(int f) {
	model_dprintf(f,"<tid=%u,",tid);
	for(unsigned int i=0;i<size;i+=2) {
		switch((ExecPointType)pairarray[i]) {
		case EP_BRANCH:
			model_dprintf(f,"br");
			break;
		case EP_COUNTER:
			model_dprintf(f,"cnt");
			break;
		case EP_LOOP:
			model_dprintf(f,"lp");
			break;
		default:
			ASSERT(false);
		}
		model_dprintf(f, "(%u)", pairarray[i+1]);
		if ((i+2)<size)
			model_dprintf(f, ", ");
	}
	model_dprintf(f, ">");
}

void ExecPoint::print() {
	model_print("<tid=%u,",tid);
	for(unsigned int i=0;i<size;i+=2) {
		switch((ExecPointType)pairarray[i]) {
		case EP_BRANCH:
			model_print("br");
			break;
		case EP_COUNTER:
			model_print("cnt");
			break;
		case EP_LOOP:
			model_print("lp");
			break;
		default:
			ASSERT(false);
		}
		model_print("(%u)", pairarray[i+1]);
		if ((i+2)<size)
			model_print(", ");
	}
	model_print(">");
}

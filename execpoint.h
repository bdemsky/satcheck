/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef EXECPOINT_H
#define EXECPOINT_H
#include "mymemory.h"
#include "threads-model.h"
#include <stdio.h>

/** @brief An execution point in a trace. These give an execution
 *  independent way of numbering steps in a trace.*/

typedef unsigned int execcount_t;
enum ExecPointType {EP_BRANCH, EP_COUNTER, EP_LOOP};
enum CompareResult {CR_BEFORE, CR_AFTER, CR_EQUALS, CR_INCOMPARABLE};

class ExecPoint {
public:
	ExecPoint(int length, thread_id_t thread_id);
	ExecPoint(ExecPoint * e);
	~ExecPoint();
	void reset();
	void pop();
	void push(ExecPointType type, execcount_t value);
	void incrementTop();
	int getSize() {return (size>>1);}
	CompareResult compare(const ExecPoint * ep) const;
	thread_id_t get_tid() const {return tid;}
	bool directInLoop();
	bool inLoop();
	ExecPointType getType();
	friend bool ExecPointEquals(ExecPoint *e1, ExecPoint * e2);
	friend unsigned int ExecPointHash(ExecPoint *e1);
	void print();
	void print(int f);
	MEMALLOC;
private:
	unsigned int length;
	unsigned int size;
	execcount_t * pairarray;
	unsigned int hashvalue;
	thread_id_t tid;
};

unsigned int ExecPointHash(ExecPoint *e);
bool ExecPointEquals(ExecPoint *e1, ExecPoint * e2);

#endif

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef SCHEDULEBUILDER_H
#define SCHEDULEBUILDER_H
#include "classlist.h"
#include "stl-model.h"

class ScheduleBuilder {
 public:
	ScheduleBuilder(MCExecution *_execution, ConstGen *cgen);
	~ScheduleBuilder();
	void buildSchedule(bool *satsolution);

	SNAPSHOTALLOC;
 private:
	EPRecord * getNextRecord(EPRecord *record);
	EPRecord * processRecord(EPRecord *record, bool * satsolution);
	ConstGen * cg;
	MCExecution *execution;
	MCScheduler *scheduler;
	SnapVector<EPRecord *> threads;
#ifdef TSO
	SnapVector<SnapList<EPRecord *> *> stores;
	SnapVector<EPRecord *> storelastoperation;
#endif
	SnapVector<EPRecord *> lastoperation;
};
#endif

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/** @file model.h
 *  @brief Core model checker.
 */

#ifndef __MODEL_H__
#define __MODEL_H__

#include <cstddef>
#include <inttypes.h>
#include "mcexecution.h"
#include "mymemory.h"
#include "hashtable.h"
#include "config.h"
#include "context.h"
#include "params.h"

class MC {
 public:
	MC(struct model_params params);
	~MC();
	MCExecution * get_execution() const { return execution; }
	MCScheduler * get_scheduler() const { return execution->get_scheduler(); }
	void check();
	const model_params params;

	MEMALLOC;
 private:
	MCExecution *execution;
	void run_execution();

	friend void user_main_wrapper();
};

extern MC *model;
#endif /* __MODEL_H__ */

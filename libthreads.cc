/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include <threads.h>
#include "common.h"
#include "threads-model.h"

/* global "model" object */
#include "model.h"

/*
 * User program API functions
 */
int thrd_create(thrd_t *t, thrd_start_t start_routine, void *arg)
{
	model->get_execution()->threadCreate(t, start_routine, arg);
	return 0;
}

int thrd_join(thrd_t t)
{
	Thread *th = t.priv;
	model->get_execution()->threadJoin(th);
	return 0;
}

void thrd_yield(void)
{
	model->get_execution()->threadYield();
}

thrd_t thrd_current(void)
{
	return thread_current()->get_thrd_t();
}

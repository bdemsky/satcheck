/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/** @file threads.cc
 *  @brief Thread functions.
 */

#include <string.h>

#include <threads.h>
#include <mutex>
#include "common.h"
#include "threads-model.h"
#include "mcschedule.h"
/* global "model" object */
#include "model.h"

/** Allocate a stack for a new thread. */
static void * stack_allocate(size_t size)
{
	return snapshot_malloc(size);
}

/** Free a stack for a terminated thread. */
static void stack_free(void *stack)
{
	snapshot_free(stack);
}

/**
 * @brief Get the current Thread
 *
 * Must be called from a user context
 *
 * @return The currently executing thread
 */
Thread * thread_current(void)
{
	ASSERT(model);
	return model->get_execution()->get_current_thread();
}

/**
 * Provides a startup wrapper for each thread, allowing some initial
 * model-checking data to be recorded. This method also gets around makecontext
 * not being 64-bit clean
 */
void thread_startup()
{
	Thread * curr_thread = thread_current();
	model->get_execution()->threadStart(curr_thread->getParentRecord());
	/* Call the actual thread function */
	curr_thread->start_routine(curr_thread->arg);
	model->get_execution()->threadFinish();
}

/**
 * Create a thread context for a new thread so we can use
 * setcontext/getcontext/swapcontext to swap it out.
 * @return 0 on success; otherwise, non-zero error condition
 */
int Thread::create_context()
{
	int ret;

	ret = getcontext(&context);
	if (ret)
		return ret;

	/* Initialize new managed context */
	stack = stack_allocate(STACK_SIZE);
	context.uc_stack.ss_sp = stack;
	context.uc_stack.ss_size = STACK_SIZE;
	context.uc_stack.ss_flags = 0;
	context.uc_link = model->get_scheduler()->get_system_context();
	makecontext(&context, thread_startup, 0);

	return 0;
}


/** Terminate a thread and free its stack. */
void Thread::complete()
{
	ASSERT(!is_complete());
	DEBUG("completed thread %d\n", id_to_int(get_id()));
	state = THREAD_COMPLETED;
}

/**
 * @brief Construct a new model-checker Thread
 *
 * A model-checker Thread is used for accounting purposes only. It will never
 * have its own stack, and it should never be inserted into the Scheduler.
 *
 * @param tid The thread ID to assign
 */
Thread::Thread(thread_id_t tid) :
	parent(NULL),
	start_routine(NULL),
	arg(NULL),
	stack(NULL),
	user_thread(NULL),
	waiting(NULL),
	id(tid),
	state(THREAD_READY),			/* Thread is always ready? */
	model_thread(true)
{
	memset(&context, 0, sizeof(context));
}

/**
 * Construct a new thread.
 * @param t The thread identifier of the newly created thread.
 * @param func The function that the thread will call.
 * @param a The parameter to pass to this function.
 */
Thread::Thread(thread_id_t tid, thrd_t *t, void (*func)(void *), void *a, Thread *parent, EPRecord *record) :
	parent(parent),
	parentrecord(record),
	start_routine(func),
	arg(a),
	user_thread(t),
	waiting(NULL),
	id(tid),
	state(THREAD_CREATED),
	model_thread(false)
{
	int ret;

	/* Initialize state */
	ret = create_context();
	if (ret)
		model_print("Error in create_context\n");

	user_thread->priv = this;
}

/** Destructor */
Thread::~Thread()
{
	if (!is_complete())
		complete();
	if (stack)
		stack_free(stack);
}

/** @return The thread_id_t corresponding to this Thread object. */
thread_id_t Thread::get_id() const
{
	return id;
}

/**
 * Set a thread's THREAD_* state (@see thread_state)
 * @param s The state to enter
 */
void Thread::set_state(thread_state s)
{
	ASSERT(s == THREAD_COMPLETED || state != THREAD_COMPLETED);
	state = s;
}

/**
 * Get the Thread that this Thread is immediately waiting on
 * @return The thread we are waiting on, if any; otherwise NULL
 */
Thread * Thread::waiting_on()
{
	return waiting;
}

void Thread::set_waiting(Thread *th) {
	waiting=th;
}

/**
 * Check if this Thread is waiting (blocking) on a given Thread, directly or
 * indirectly (via a chain of waiting threads)
 *
 * @param t The Thread on which we may be waiting
 * @return True if we are waiting on Thread t; false otherwise
 */
bool Thread::is_waiting_on(const Thread *t) {
	Thread *wait;
	for (wait = waiting_on();wait != NULL;wait = wait->waiting_on())
		if (wait == t)
			return true;
	return false;
}

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/** @file threads-model.h
 *  @brief Model Checker Thread class.
 */

#ifndef __THREADS_MODEL_H__
#define __THREADS_MODEL_H__

#include <stdint.h>

#include "mymemory.h"
#include <threads.h>
#include "modeltypes.h"
#include "stl-model.h"
#include "context.h"
#include "classlist.h"

struct thread_params {
	thrd_start_t func;
	void *arg;
};

/** @brief Represents the state of a user Thread */
typedef enum thread_state {
	/** Thread was just created and hasn't run yet */
	THREAD_CREATED,
	/** Thread is running */
	THREAD_RUNNING,
	/** Thread is not currently running but is ready to run */
	THREAD_READY,
	/**
	 * Thread is waiting on another action (e.g., thread completion, lock
	 * release, etc.)
	 */
	THREAD_BLOCKED,
	/** Thread has completed its execution */
	THREAD_COMPLETED
} thread_state;


/** @brief A Thread is created for each user-space thread */
class Thread {
public:
	Thread(thread_id_t tid);
	Thread(thread_id_t tid, thrd_t *t, void (*func)(void *), void *a, Thread *parent, EPRecord *r);
	~Thread();
	void complete();

	ucontext_t * get_context() {
		return &context;
	}

	thread_state get_state() const { return state; }
	void set_state(thread_state s);
	thread_id_t get_id() const;
	thrd_t get_thrd_t() const { return *user_thread; }
	Thread * get_parent() const { return parent; }

	/** @return True if this thread is finished executing */
	bool is_complete() const { return state == THREAD_COMPLETED; }

	/** @return True if this thread is blocked */
	bool is_blocked() const { return state == THREAD_BLOCKED; }

	Thread * waiting_on();
	void set_waiting(Thread *);

	bool is_waiting_on(const Thread *t);

	bool is_model_thread() const { return model_thread; }

	friend void thread_startup();

	/**
	 * Intentionally NOT allocated with MODELALLOC or SNAPSHOTALLOC.
	 * Threads should be allocated on the user's normal (snapshotting) heap
	 * to allow their allocation/deallocation to follow the same pattern as
	 * the rest of the backtracked/replayed program.
	 */
	void * operator new(size_t size) {
		return Thread_malloc(size);
	}
	void operator delete(void *p, size_t size) {
		Thread_free(p);
	}
	void * operator new[](size_t size) {
		return Thread_malloc(size);
	}
	void operator delete[](void *p, size_t size) {
		Thread_free(p);
	}
	EPRecord * getParentRecord() {return parentrecord;}
private:
	int create_context();

	/** @brief The parent Thread which created this Thread */
	Thread * const parent;
	EPRecord * parentrecord;
	void (*start_routine)(void *);
	void *arg;
	ucontext_t context;
	void *stack;
	thrd_t *user_thread;
	Thread *waiting;
	thread_id_t id;
	thread_state state;

	/** @brief Is this Thread a special model-checker thread? */
	const bool model_thread;
};

Thread * thread_current();

static inline thread_id_t thrd_to_id(thrd_t t)
{
	return t.priv->get_id();
}

/**
 * @brief Map a zero-based integer index to a unique thread ID
 *
 * This is the inverse of id_to_int
 */
static inline thread_id_t int_to_id(int i)
{
	return i;
}

/**
 * @brief Map a unique thread ID to a zero-based integer index
 *
 * This is the inverse of int_to_id
 */
static inline int id_to_int(thread_id_t id)
{
	return id;
}

#endif/* __THREADS_MODEL_H__ */

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <new>

#include "mymemory.h"
#include "snapshot.h"
#include "common.h"
#include "threads-model.h"
#include "model.h"
#include "mcexecution.h"

#define REQUESTS_BEFORE_ALLOC 1024

size_t allocatedReqs[REQUESTS_BEFORE_ALLOC] = { 0 };
int nextRequest = 0;
int howManyFreed = 0;
#if !USE_MPROTECT_SNAPSHOT
static mspace sStaticSpace = NULL;
#endif

/** Non-snapshotting calloc for our use. */
void *model_calloc(size_t count, size_t size)
{
#if USE_MPROTECT_SNAPSHOT
	static void *(*callocp)(size_t count, size_t size) = NULL;
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!callocp) {
		callocp = (void * (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = callocp(count, size);
	return ptr;
#else
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_calloc(sStaticSpace, count, size);
#endif
}

/** Non-snapshotting malloc for our use. */
void *model_malloc(size_t size)
{
#if USE_MPROTECT_SNAPSHOT
	static void *(*mallocp)(size_t size) = NULL;
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!mallocp) {
		mallocp = (void * (*)(size_t))dlsym(RTLD_NEXT, "malloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = mallocp(size);
	return ptr;
#else
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_malloc(sStaticSpace, size);
#endif
}

/** @brief Snapshotting realloc, for use by model-checker (not user progs) */
void *model_realloc(void *ptr, size_t size)
{
#if USE_MPROTECT_SNAPSHOT
	static void *(*reallocp)(void *ptr, size_t size) = NULL;
	char *error;
	void *tmpptr;

	/* get address of libc malloc */
	if (!reallocp) {
		reallocp = (void * (*)(void *,size_t))dlsym(RTLD_NEXT, "realloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	tmpptr = reallocp(ptr, size);
	return tmpptr;
#else
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_realloc(sStaticSpace, ptr, size);
#endif
}

/** @brief Snapshotting malloc, for use by model-checker (not user progs) */
void * snapshot_malloc(size_t size)
{
	void *tmp = mspace_malloc(model_snapshot_space, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting calloc, for use by model-checker (not user progs) */
void * snapshot_calloc(size_t count, size_t size)
{
	void *tmp = mspace_calloc(model_snapshot_space, count, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting realloc, for use by model-checker (not user progs) */
void *snapshot_realloc(void *ptr, size_t size)
{
	void *tmp = mspace_realloc(model_snapshot_space, ptr, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting free, for use by model-checker (not user progs) */
void snapshot_free(void *ptr)
{
	mspace_free(model_snapshot_space, ptr);
}

/** Non-snapshotting free for our use. */
void model_free(void *ptr)
{
#if USE_MPROTECT_SNAPSHOT
	static void (*freep)(void *);
	char *error;

	/* get address of libc free */
	if (!freep) {
		freep = (void (*)(void *))dlsym(RTLD_NEXT, "free");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	freep(ptr);
#else
	mspace_free(sStaticSpace, ptr);
#endif
}

/** Bootstrap allocation. Problem is that the dynamic linker calls require
 *  calloc to work and calloc requires the dynamic linker to work. */

#define BOOTSTRAPBYTES 4096
char bootstrapmemory[BOOTSTRAPBYTES];
size_t offset = 0;

void * HandleEarlyAllocationRequest(size_t sz)
{
	/* Align to 8 byte boundary */
	sz = (sz + 7) & ~7;

	if (sz > (BOOTSTRAPBYTES-offset)) {
		model_print("OUT OF BOOTSTRAP MEMORY\n");
		exit(EXIT_FAILURE);
	}

	void *pointer = (void *)&bootstrapmemory[offset];
	offset += sz;
	return pointer;
}

/** @brief Global mspace reference for the model-checker's snapshotting heap */
mspace model_snapshot_space = NULL;

#if USE_MPROTECT_SNAPSHOT

/** @brief Global mspace reference for the user's snapshotting heap */
void * user_snapshot_space = NULL;
mspace thread_snapshot_space = NULL;

struct snapshot_heap_data * snapshot_struct;

/** Check whether this is bootstrapped memory that we should not free */
static bool DontFree(void *ptr)
{
	return (ptr >= (&bootstrapmemory[0]) && ptr < (&bootstrapmemory[BOOTSTRAPBYTES]));
}


static void * user_malloc(size_t size) {
	return model->get_execution()->alloc(size);
}

/**
 * @brief The allocator function for "user" allocation
 *
 * Should only be used for allocations which will not disturb the allocation
 * patterns of a user thread.
 */
void * real_user_malloc(size_t size)
{
	size=(size+7)&~((size_t)7);
	void *tmp = snapshot_struct->allocation_ptr;
	snapshot_struct->allocation_ptr = (void *)((char *) snapshot_struct->allocation_ptr +size);
	
	ASSERT(snapshot_struct->allocation_ptr <= snapshot_struct->top_ptr);
	return tmp;
}

/**
 * @brief Snapshotting malloc implementation for user programs
 *
 * Do NOT call this function from a model-checker context. Doing so may disrupt
 * the allocation patterns of a user thread.
 */
void *malloc(size_t size)
{
	if (user_snapshot_space) {
		/* Only perform user allocations from user context */
		return user_malloc(size);
	} else
		return HandleEarlyAllocationRequest(size);
}

/** @brief Snapshotting free implementation for user programs */
void free(void * ptr)
{
	if (!DontFree(ptr))
		mspace_free(user_snapshot_space, ptr);
}

/** @brief Snapshotting realloc implementation for user programs */
void *realloc(void *ptr, size_t size)
{
	ASSERT(false);
	return NULL;
}

/** @brief Snapshotting calloc implementation for user programs */
void * calloc(size_t num, size_t size)
{
	if (user_snapshot_space) {
		void *tmp = user_malloc(num * size);
		bzero(tmp, num*size);
		ASSERT(tmp);
		return tmp;
	} else {
		void *tmp = HandleEarlyAllocationRequest(size * num);
		memset(tmp, 0, size * num);
		return tmp;
	}
}

/** @brief Snapshotting allocation function for use by the Thread class only */
void * Thread_malloc(size_t size)
{
	void *tmp = mspace_malloc(thread_snapshot_space, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting free function for use by the Thread class only */
void Thread_free(void *ptr)
{
	free(ptr);
}

/** @brief Snapshotting new operator for user programs */
void * operator new(size_t size) throw(std::bad_alloc)
{
	return malloc(size);
}

/** @brief Snapshotting delete operator for user programs */
void operator delete(void *p) throw()
{
	free(p);
}

/** @brief Snapshotting new[] operator for user programs */
void * operator new[](size_t size) throw(std::bad_alloc)
{
	return malloc(size);
}

/** @brief Snapshotting delete[] operator for user programs */
void operator delete[](void *p, size_t size)
{
	free(p);
}

#else /* !USE_MPROTECT_SNAPSHOT */

/** @brief Snapshotting allocation function for use by the Thread class only */
void * Thread_malloc(size_t size)
{
	return malloc(size);
}

/** @brief Snapshotting free function for use by the Thread class only */
void Thread_free(void *ptr)
{
	free(ptr);
}

#endif /* !USE_MPROTECT_SNAPSHOT */

#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <stdbool.h>
#include "libinterface.h"

/** Example implementation of linux rw lock along with 2 thread test
 *  driver... */

typedef union {
	int lock;
} rwlock_t;

static inline bool write_trylock(rwlock_t *rw)
{
	int priorvalue=rmw_32(CAS, &rw->lock, 0, 1);
	return priorvalue;
}

static inline void write_unlock(rwlock_t *rw)
{
	store_32(&rw->lock, 0);
}

rwlock_t * mylock;

static void foo() {
	int flag=write_trylock(mylock);
	if (flag) {
	} else {
		write_unlock(mylock);
	}
}

static void a(void *obj)
{
	int i;
	for(i=0;i<PROBLEMSIZE;i++)
		foo();
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;//, t3, t4;
	mylock = (rwlock_t*)malloc(sizeof(rwlock_t));
	store_32(&mylock->lock, 0);

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&a, NULL);
	//thrd_create(&t3, (thrd_start_t)&a, NULL);
	//thrd_create(&t4, (thrd_start_t)&a, NULL);

	thrd_join(t1);
	thrd_join(t2);
	//thrd_join(t3);
	//thrd_join(t4);

	return 0;
}

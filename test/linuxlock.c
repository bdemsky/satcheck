#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "libinterface.h"


/** Example implementation of linux rw lock along with 2 thread test
 *  driver... */

typedef union {
	int lock;
} rwlock_t;

static inline bool write_trylock(rwlock_t *rw, MCID *retval)
{
	MCID m_priorval=MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
	int priorvalue=rmw_32(CAS, &rw->lock, 0, 1);
	*retval=m_priorval;
	return priorvalue;
}

static inline void write_unlock(rwlock_t *rw)
{
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&rw->lock, 0);
}

rwlock_t mylock;

static void foo() {
	MCID val;
	bool flag=write_trylock(&mylock,&val);
	MCID br;
	if (flag) {
		br=MC2_branchUsesID(val, 1, 2, true);
		MC2_merge(br);
	} else {
		br=MC2_branchUsesID(val, 0, 2, true);
		write_unlock(&mylock);
		MC2_merge(br);
	}
}

static void a(void *obj)
{
	int i;
	for(i=0;i<2;i++)
		foo();
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;//, t3, t4;
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&mylock.lock, 0);

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

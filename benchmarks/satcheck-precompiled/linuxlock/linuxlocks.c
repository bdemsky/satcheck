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

static inline bool write_trylock(MCID _mrw, rwlock_t *rw, MCID * retval)
{
	MCID _rmw0 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
	int priorvalue=rmw_32(CAS, &rw->lock, 0, 1);
	*retval = _rmw0;
	return priorvalue;
}

static inline void write_unlock(MCID _mrw, rwlock_t *rw)
{
	MC2_nextOpStoreOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP);
	store_32(&rw->lock, 0);
}

MCID _fn0; rwlock_t * mylock;

static void foo() {
	MCID _rv0;
	int flag=write_trylock(MCID_NODEP, mylock, &_rv0);
	MCID _br0;
	
	int _cond0 = flag;
	MCID _cond0_m = MC2_function_id(1, 1, sizeof(_cond0), _cond0, _rv0);
	if (_cond0) {_br0 = MC2_branchUsesID(_rv0, 1, 2, true);
	
		MC2_merge(_br0);
	} else {
		_br0 = MC2_branchUsesID(_rv0, 0, 2, true);
		write_unlock(MCID_NODEP, mylock);
		MC2_merge(_br0);
	}
}

static void a(void *obj)
{
	int i;
	MC2_enterLoop();
	for(i=0;i<PROBLEMSIZE;i++)
		foo();
MC2_exitLoop();

}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;//, t3, t4;
	mylock = (rwlock_t*)malloc(sizeof(rwlock_t));
	_fn0 = MC2_function_id(0, 0, sizeof (mylock), (uint64_t)mylock); 
	MC2_nextOpStoreOffset(_fn0, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP);
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

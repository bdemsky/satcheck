#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "libinterface.h"

#define RW_LOCK_BIAS            0x00100000
#define WRITE_LOCK_CMP          RW_LOCK_BIAS

/** Example implementation of linux rw lock along with 2 thread test
 *  driver... */

typedef union {
	int lock;
} rwlock_t;

static inline void read_lock(MCID _mrw, rwlock_t *rw)
{
	MCID _rmw0 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
	int priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-1));

	MC2_enterLoop();
	while (true) {
		MCID _br0;
		
		int _cond0 = priorvalue <= 0;
		MCID _cond0_m = MC2_function_id(1, 1, sizeof(_cond0), _cond0, _rmw0);
		if (_cond0) {_br0 = MC2_branchUsesID(_cond0_m, 1, 2, true);
		
			MC2_merge(_br0);
		} else {
			_br0 = MC2_branchUsesID(_cond0_m, 0, 2, true);
			break;
		}

		MCID _rmw1 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
		rmw_32(ADD, &rw->lock, /* dummy */ 0, 1);

		MC2_enterLoop();
		while (true) {
			MCID _mstatus; _mstatus=MC2_nextOpLoadOffset(_mrw, MC2_OFFSET(rwlock_t *, lock)); int status = load_32(&rw->lock);
			MCID _br1;
			
			int _cond1 = status > 0;
			MCID _cond1_m = MC2_function_id(2, 1, sizeof(_cond1), _cond1, _mstatus);
			if (_cond1) {_br1 = MC2_branchUsesID(_cond1_m, 1, 2, true);
			
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_cond1_m, 0, 2, true);
				break;
			}
			MC2_yield();
		}
MC2_exitLoop();


		
		MCID _rmw2 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
		priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-1));
	}
MC2_exitLoop();

}

static inline void write_lock(MCID _mrw, rwlock_t *rw)
{
	MCID _rmw3 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
	int priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-(RW_LOCK_BIAS)));
	MC2_enterLoop();
	while(true) {
		MCID _br2;
		
		int _cond2 = priorvalue != 1048576;
		MCID _cond2_m = MC2_function_id(3, 1, sizeof(_cond2), _cond2, _rmw3);
		if (_cond2) {_br2 = MC2_branchUsesID(_cond2_m, 1, 2, true);
		
			MC2_merge(_br2);
		} else {
			_br2 = MC2_branchUsesID(_cond2_m, 0, 2, true);
			break;
		}

		MCID _rmw4 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
		rmw_32(ADD, &rw->lock, /* dummy */ 0, RW_LOCK_BIAS);
		
		MC2_enterLoop();
		while (true) {
			MCID _mstatus; _mstatus=MC2_nextOpLoadOffset(_mrw, MC2_OFFSET(rwlock_t *, lock)); int status = load_32(&rw->lock);
			MCID _br3;
			
			int _cond3 = status != 1048576;
			MCID _cond3_m = MC2_function_id(4, 1, sizeof(_cond3), _cond3, _mstatus);
			if (_cond3) {_br3 = MC2_branchUsesID(_cond3_m, 1, 2, true);
			
				MC2_merge(_br3);
			} else {
				_br3 = MC2_branchUsesID(_cond3_m, 0, 2, true);
				break;
			}
			MC2_yield();
		}
MC2_exitLoop();


		MCID _rmw5 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
		priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-(RW_LOCK_BIAS)));
	}
MC2_exitLoop();

}

static inline bool write_trylock(MCID _mrw, rwlock_t *rw, MCID * retval)
{
	MCID _rmw6 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
	int priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-(RW_LOCK_BIAS)));

	MCID _fn0; int flag = priorvalue != RW_LOCK_BIAS;
	_fn0 = MC2_function_id(7, 1, sizeof (flag), (uint64_t)flag, _rmw6); 
	MCID _br4;
	
	int _cond4 = !flag;
	MCID _cond4_m = MC2_function_id(5, 1, sizeof(_cond4), _cond4, _fn0);
	if (_cond4) {
		_br4 = MC2_branchUsesID(_cond4_m, 1, 2, true);
		MCID _rmw7 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
		rmw_32(ADD, &rw->lock, /* dummy */ 0, RW_LOCK_BIAS);
		MC2_merge(_br4);
	}
 else { _br4 = MC2_branchUsesID(_cond4_m, 0, 2, true);	MC2_merge(_br4);
 }
	*retval = _fn0;
	return flag;
}

static inline void read_unlock(MCID _mrw, rwlock_t *rw)
{
	MCID _rmw8 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
	rmw_32(ADD, &rw->lock, /* dummy */ 0, 1);
}

static inline void write_unlock(MCID _mrw, rwlock_t *rw)
{
	MCID _rmw9 = MC2_nextRMWOffset(_mrw, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP, MCID_NODEP);
	rmw_32(ADD, &rw->lock, /* dummy */ 0, RW_LOCK_BIAS);
}

MCID _fn1; rwlock_t * mylock;
int shareddata;

static void foo() {
	MCID _rv0;
	int res = write_trylock(MCID_NODEP, mylock, &_rv0);
	MCID _br5;
	
	int _cond5 = res;
	MCID _cond5_m = MC2_function_id(6, 1, sizeof(_cond5), _cond5, _rv0);
	if (_cond5) {
		_br5 = MC2_branchUsesID(_rv0, 1, 2, true);
		write_unlock(MCID_NODEP, mylock);
		MC2_merge(_br5);
	} else {_br5 = MC2_branchUsesID(_rv0, 0, 2, true);
	
		MC2_merge(_br5);
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
	mylock = (rwlock_t*)malloc(sizeof(rwlock_t));

_fn1 = MC2_function_id(0, 0, sizeof (mylock), (uint64_t)mylock); 
	thrd_t t1, t2;
	//, t3, t4;
	MC2_nextOpStoreOffset(_fn1, MC2_OFFSET(rwlock_t *, lock), MCID_NODEP);
	store_32(&mylock->lock, RW_LOCK_BIAS);

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&a, NULL);
	//	thrd_create(&t3, (thrd_start_t)&a, NULL);
	//	thrd_create(&t4, (thrd_start_t)&a, NULL);

	thrd_join(t1);
	thrd_join(t2);
	//	thrd_join(t3);
	//	thrd_join(t4);
	free(mylock);

	return 0;
}

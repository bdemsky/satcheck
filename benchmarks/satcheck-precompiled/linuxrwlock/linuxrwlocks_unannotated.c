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

static inline void read_lock(rwlock_t *rw)
{
	int priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-1));

	while (true) {
		if (priorvalue <= 0) {
		} else {
			break;
		}

		rmw_32(ADD, &rw->lock, /* dummy */ 0, 1);

		while (true) {
			int status = load_32(&rw->lock);
			if (status > 0) {
			} else {
				break;
			}
			MC2_yield();
		}

		
		priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-1));
	}
}

static inline void write_lock(rwlock_t *rw)
{
	int priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-(RW_LOCK_BIAS)));
	while(true) {
		if (priorvalue != RW_LOCK_BIAS) {
		} else {
			break;
		}

		rmw_32(ADD, &rw->lock, /* dummy */ 0, RW_LOCK_BIAS);
		
		while (true) {
			int status = load_32(&rw->lock);
			if (status != RW_LOCK_BIAS) {
			} else {
				break;
			}
			MC2_yield();
		}

		priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-(RW_LOCK_BIAS)));
	}
}

static inline bool write_trylock(rwlock_t *rw)
{
	int priorvalue=rmw_32(ADD, &rw->lock, /* dummy */ 0, ((unsigned int)-(RW_LOCK_BIAS)));

	int flag = priorvalue != RW_LOCK_BIAS;
	if (!flag) {
		rmw_32(ADD, &rw->lock, /* dummy */ 0, RW_LOCK_BIAS);
	}

	return flag;
}

static inline void read_unlock(rwlock_t *rw)
{
	rmw_32(ADD, &rw->lock, /* dummy */ 0, 1);
}

static inline void write_unlock(rwlock_t *rw)
{
	rmw_32(ADD, &rw->lock, /* dummy */ 0, RW_LOCK_BIAS);
}

rwlock_t * mylock;
int shareddata;

static void foo() {
	int res = write_trylock(mylock);
	if (res) {
		write_unlock(mylock);
	} else {
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
	mylock = (rwlock_t*)malloc(sizeof(rwlock_t));

	thrd_t t1, t2;
	//, t3, t4;
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

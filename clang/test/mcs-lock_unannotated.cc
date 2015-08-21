#include <stdio.h>
#include <threads.h>

#include "libinterface.h"

// mcs on stack

struct mcs_node {
	mcs_node * next;
	int gate;

	mcs_node() {
		store_64(&next, 0);
		store_32(&gate, 0);
	}
};

struct mcs_mutex {
public:
	// tail is null when lock is not held
	mcs_node * m_tail;

	mcs_mutex() {
		store_64(&m_tail, (uint64_t)NULL);
	}
	~mcs_mutex() {
		// ASSERT( m_tail.load() == NULL );
	}

	class guard {
	public:
		mcs_mutex * m_t;
		mcs_node    m_node; // node held on the stack

		guard(mcs_mutex * t) : m_t(t) { t->lock(this); }
		~guard() { m_t->unlock(this); }
	};

	void lock(guard * I) {
		mcs_node * me = &(I->m_node);

		// set up my node :
		// not published yet so relaxed :
		store_64(&me->next, (uint64_t)NULL);
		store_32(&me->gate, 1);

		// publish my node as the new tail :
		// mcs_node * pred = m_tail.exchange(me, std::mo_acq_rel);
		mcs_node * pred = (mcs_node *)rmw_64(EXC, &m_tail, (uint64_t) me, (uint64_t) NULL);
		if ( pred != NULL ) {
			// (*1) race here
			// unlock of pred can see me in the tail before I fill next

			// publish me to previous lock-holder :
			store_64(&pred->next, (uint64_t)me);

			// (*2) pred not touched any more       

			// now this is the spin -
			// wait on predecessor setting my flag -
			while ( true ) {
				if (!load_32(&me->gate)) {
					break;
				} else {
					;
				}
				thrd_yield();
			}
		}
	}

	void unlock(guard * I) {
		mcs_node * me = &(I->m_node);

		mcs_node * next = (mcs_node *)load_64(&me->next);
		if ( next == NULL )
		{
			mcs_node * tail_was_me = me;
			//if ( m_tail.compare_exchange_strong( tail_was_me,NULL,std::mo_acq_rel) ) {
			if (rmw_64(CAS, &m_tail, (uint64_t)tail_was_me, (uint64_t)NULL)) {
				// got null in tail, mutex is unlocked
				return;
			}

			// (*1) catch the race :
			while(true) {
				next = (mcs_node *)load_64(&me->next);
				if ( next != NULL ) {
					break;
				} else {
					;
				}
				thrd_yield();
			}
		}

		// (*2) - store to next must be done,
		//  so no locker can be viewing my node any more        

		// let next guy in :
    	store_32(&next->gate, 0);
	}
};

struct mcs_mutex *mutex;
static uint32_t shared;

void threadA(void *arg)
{
	mcs_mutex::guard g(mutex);
	printf("store: %d\n", 17);
	store_32(&shared, 17);
	mutex->unlock(&g);
	mutex->lock(&g);
	printf("load: %u\n", load_32(&shared));
}

void threadB(void *arg)
{
	mcs_mutex::guard g(mutex);
	printf("load: %u\n", load_32(&shared));
	mutex->unlock(&g);
	mutex->lock(&g);
	printf("store: %d\n", 17);
	store_32(&shared, 17);
}

int user_main(int argc, char **argv)
{
	thrd_t A; thrd_t B;

	mutex = new mcs_mutex();

	thrd_create(&A, &threadA, NULL);
	thrd_create(&B, &threadB, NULL);
	thrd_join(A);
	thrd_join(B);
	return 0;
}

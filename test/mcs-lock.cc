#include <stdio.h>
#include <threads.h>

#include "libinterface.h"

// mcs on stack

struct mcs_node {
	MCID _mnext; mcs_node * next;
	int gate;

	mcs_node() {
		MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
		store_64(&next, 0);
		MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
		store_32(&gate, 0);
	}
};

struct mcs_mutex {
public:
	// tail is null when lock is not held
	mcs_node * m_tail;

	mcs_mutex() {
		MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
		store_64(&m_tail, (uint64_t)NULL);
	}
	~mcs_mutex() {
		// ASSERT( m_tail.load() == NULL );
	}

	class guard {
	public:
		mcs_mutex * m_t;
		mcs_node    m_node; // node held on the stack

		guard(MCID _mt, mcs_mutex * t) : m_t(t) { t->lock(MCID_NODEP, this); }
		~guard() { m_t->unlock(MCID_NODEP, this); }
	};

	void lock(MCID _mI, guard * I) {
		MCID _mme; mcs_node * me = &(I->m_node);

		// set up my node :
		// not published yet so relaxed :
		
		void * _p0 = &me->next;
		MCID _fn0 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p0, (uint64_t)_mme); MC2_nextOpStore(_fn0, MCID_NODEP);
		store_64(_p0, (uint64_t)NULL);
		
		void * _p1 = &me->gate;
		MCID _fn1 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p1, (uint64_t)_mme); MC2_nextOpStore(_fn1, MCID_NODEP);
		store_32(_p1, 1);

		// publish my node as the new tail :
		// mcs_node * pred = m_tail.exchange(me, std::mo_acq_rel);
		MCID _rmw0 = MC2_nextRMW(MCID_NODEP, _mme, MCID_NODEP);
		MCID _mpred; mcs_node * pred = (mcs_node *)rmw_64(EXC, &m_tail, (uint64_t) me, (uint64_t) NULL);
		MCID _br0;
		if ( pred != NULL ) {
			// (*1) race here
			// unlock of pred can see me in the tail before I fill next

			// publish me to previous lock-holder :
			_br0 = MC2_branchUsesID(_rmw0, 1, 2, true);
			
			void * _p2 = &pred->next;
			MCID _fn2 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p2, (uint64_t)_mpred); MC2_nextOpStore(_fn2, _mme);
			store_64(_p2, (uint64_t)me);

			// (*2) pred not touched any more       

			// now this is the spin -
			// wait on predecessor setting my flag -
			MC2_enterLoop();
			while ( true ) {
				MCID _br1;
				void * _p99 = &me->gate;
				MCID _fn99 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p99, (uint64_t)_mme); MC2_nextOpLoad(_fn99);
				int c88 = !load_32(_p99);
				MCID _fn88 = MC2_function(1, MC2_PTR_LENGTH, c88, _p99);
				if (c88) {
					_br1 = MC2_branchUsesID(_fn88, 1, 2, true);
					break;
				} else {
					_br1 = MC2_branchUsesID(_fn88, 0, 2, true);
					;
					MC2_merge(_br1);
				}
				thrd_yield();
			}
MC2_exitLoop();

			MC2_merge(_br0);
		} else { _br0 = MC2_branchUsesID(_rmw0, 0, 2, true);	MC2_merge(_br0);
		 }
	}

	void unlock(MCID _mI, guard * I) {
		MCID _mme; mcs_node * me;
		me = &(I->m_node);

		MCID _fn6; 
		void * _p3 = &me->next;
		MCID _mnext; MCID _fn3 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p3, _mme); _mnext=MC2_nextOpLoad(_fn3); mcs_node * next = (mcs_node *)load_64(_p3);
		MCID _br2;
		if ( next == NULL )
		{
			_br2 = MC2_branchUsesID(_mnext, 1, 2, true);
			mcs_node * tail_was_me = me;
			//if ( m_tail.compare_exchange_strong( tail_was_me,NULL,std::mo_acq_rel) ) {
			MCID _br3;
			MCID _rmw77 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
			if (rmw_64(CAS, &m_tail, (uint64_t)tail_was_me, (uint64_t)NULL)) {
				// got null in tail, mutex is unlocked
				_br3 = MC2_branchUsesID(_rmw77, 1, 2, true);
				return;
			} else { _br3 = MC2_branchUsesID(_rmw77, 0, 2, true);	MC2_merge(_br3);
			 }

			// (*1) catch the race :
			MC2_enterLoop();
			while(true) {
				
				void * _p4 = &me->next;
				MCID _fn4 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p4, _mme); _fn6=MC2_nextOpLoad(_fn4), next = (mcs_node *)load_64(_p4);
				_fn6 = MC2_function(1, sizeof (next), (uint64_t)next, (uint64_t)_fn6); 
				MCID _br4;
				if ( next != NULL ) {
					_br4 = MC2_branchUsesID(_fn6, 1, 2, true);
					break;
				} else {
					_br4 = MC2_branchUsesID(_fn6, 0, 2, true);
					;
					MC2_merge(_br4);
				}
				thrd_yield();
			}
MC2_exitLoop();

			MC2_merge(_br2);
		} else { _br2 = MC2_branchUsesID(_mnext, 0, 2, true);	MC2_merge(_br2);
		 }

		// (*2) - store to next must be done,
		//  so no locker can be viewing my node any more        

		// let next guy in :
    	
    	void * _p5 = &next->gate;
    	MCID _fn5 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p5, _fn6); MC2_nextOpStore(_fn5, MCID_NODEP);
    	store_32(_p5, 0);
	}
};

struct mcs_mutex *mutex;
static uint32_t shared;

void threadA(void *arg)
{
	mcs_mutex::guard g(MCID_NODEP, mutex);
	printf("store: %d\n", 17);
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&shared, 17);
	mutex->unlock(MCID_NODEP, &g);
	mutex->lock(MCID_NODEP, &g);
	printf("load: %u\n", load_32(&shared));
}

void threadB(void *arg)
{
	mcs_mutex::guard g(MCID_NODEP, mutex);
	printf("load: %u\n", load_32(&shared));
	mutex->unlock(MCID_NODEP, &g);
	mutex->lock(MCID_NODEP, &g);
	printf("store: %d\n", 17);
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
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

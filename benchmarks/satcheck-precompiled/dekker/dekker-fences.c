/*
 * Dekker's critical section algorithm, implemented with fences.
 * Translated to C by Patrick Lam <prof.lam@gmail.com>
 *
 * URL:
 *   http://www.justsoftwaresolutions.co.uk/threading/
 */

#include <threads.h>
#include <stdbool.h>
#include "libinterface.h"

/* atomic */ int flag0, flag1;
/* atomic */ int turn;

#define true 1
#define false 0
#define NULL 0

/* uint32_t */ int var = 0;

void p0() {
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag0, true);
	// std::atomic_thread_fence(std::memory_order_seq_cst);

	MCID _mf1; int f1;
	MC2_enterLoop();
	while (true)
	{
		_mf1=MC2_nextOpLoad(MCID_NODEP), f1 = load_32(&flag1);
		MCID _br0;
		
		int _cond0 = !f1;
		MCID _cond0_m = MC2_function_id(1, 1, sizeof(_cond0), _cond0, _mf1);
		if (_cond0) {
			_br0 = MC2_branchUsesID(_cond0_m, 1, 2, true);
			break;
		}

	 else { _br0 = MC2_branchUsesID(_cond0_m, 0, 2, true);	MC2_merge(_br0);
		 }	MCID _br1;
		MCID _m_cond1_m=MC2_nextOpLoad(MCID_NODEP); 
		int _cond1 = load_32(&turn);
		MCID _cond1_m = MC2_function_id(2, 1, sizeof(_cond1), _cond1, _m_cond1_m);
		if (_cond1) {
			_br1 = MC2_branchUsesID(_cond1_m, 1, 2, true);
			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag0, false);
			MC2_enterLoop();
			while(1) {
				MCID _br2;
				MCID _m_cond2_m=MC2_nextOpLoad(MCID_NODEP); 
				int _cond2 = !load_32(&turn);
				MCID _cond2_m = MC2_function_id(3, 1, sizeof(_cond2), _cond2, _m_cond2_m);
				if (_cond2) {
					_br2 = MC2_branchUsesID(_cond2_m, 1, 2, true);
					break;
				}
		 else { _br2 = MC2_branchUsesID(_cond2_m, 0, 2, true);	MC2_merge(_br2);
				 }		
				MC2_yield();
			}
MC2_exitLoop();


			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag0, true);
			MC2_merge(_br1);
		} else {
			_br1 = MC2_branchUsesID(_cond1_m, 0, 2, true);
			MC2_yield();
			MC2_merge(_br1);
		}
	}
MC2_exitLoop();


	// std::atomic_thread_fence(std::memory_order_acquire);

	// critical section
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&var, 1);

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&turn, 1);
	// std::atomic_thread_fence(std::memory_order_release);
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag0, false);
}

void p1() {
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag1, true);
	// std::atomic_thread_fence(std::memory_order_seq_cst);
	
	int f0;
	MC2_enterLoop();
	while (true) {
		MCID _mf0; _mf0=MC2_nextOpLoad(MCID_NODEP); int f0 = load_32(&flag0);
		MCID _br3;
		
		int _cond3 = !f0;
		MCID _cond3_m = MC2_function_id(4, 1, sizeof(_cond3), _cond3, _mf0);
		if (_cond3) {
			_br3 = MC2_branchUsesID(_cond3_m, 1, 2, true);
			break;
		}
		 else { _br3 = MC2_branchUsesID(_cond3_m, 0, 2, true);	MC2_merge(_br3);
		 }
		MCID _br4;
		MCID _m_cond4_m=MC2_nextOpLoad(MCID_NODEP); 
		int _cond4 = !load_32(&turn);
		MCID _cond4_m = MC2_function_id(5, 1, sizeof(_cond4), _cond4, _m_cond4_m);
		if (_cond4) {
			_br4 = MC2_branchUsesID(_cond4_m, 1, 2, true);
			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag1, false);
			MC2_enterLoop();
			while (1) {
				MCID _br5;
				MCID _m_cond5_m=MC2_nextOpLoad(MCID_NODEP); 
				int _cond5 = load_32(&turn);
				MCID _cond5_m = MC2_function_id(6, 1, sizeof(_cond5), _cond5, _m_cond5_m);
				if (_cond5)
					{_br5 = MC2_branchUsesID(_cond5_m, 1, 2, true);
					break;
}	 else { _br5 = MC2_branchUsesID(_cond5_m, 0, 2, true);	MC2_merge(_br5);
				 }			MC2_yield();
			}
MC2_exitLoop();

			
			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag1, true);
			// std::atomic_thread_fence(std::memory_order_seq_cst);
			MC2_merge(_br4);
		}
 else { _br4 = MC2_branchUsesID(_cond4_m, 0, 2, true);	MC2_merge(_br4);
	 }	}
MC2_exitLoop();


	// std::atomic_thread_fence(std::memory_order_acquire);

	// critical section
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&var, 2);

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&turn, 0);
	// std::atomic_thread_fence(std::memory_order_release);
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag1, false);
}

void p0l(void *a) {
	int i;
	MC2_enterLoop();
	for(i=0;i<PROBLEMSIZE;i++) {
		p0();
	}
MC2_exitLoop();

}


void p1l(void *a) {
	int i;
	MC2_enterLoop();
	for(i=0;i<PROBLEMSIZE;i++) {
		p1();
	}
MC2_exitLoop();

}


int user_main(int argc, char **argv)
{
	thrd_t a, b;

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag0, false);

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag1, false);

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&turn, 0);

	thrd_create(&a, p0l, NULL);
	thrd_create(&b, p1l, NULL);

	thrd_join(a);
	thrd_join(b);

	return 0;
}

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

void p0(void *arg)
{
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag0, true);
	// std::atomic_thread_fence(std::memory_order_seq_cst);

	MCID _mt; MCID _fn0; int f1, t;
	MC2_enterLoop();
	while (true)
	{
		_fn0=MC2_nextOpLoad(MCID_NODEP), f1 = load_32(&flag1);
		_fn0 = MC2_function(1, sizeof (f1), f1, _fn0); 
		MCID _br0;
		if (!f1) {
			_br0 = MC2_branchUsesID(_fn0, 1, 2, true);
			break;
		} else {
			_br0 = MC2_branchUsesID(_fn0, 0, 2, true);
			;
			MC2_merge(_br0);
		}

		if ((_mt=MC2_nextOpLoad(MCID_NODEP), t = load_32(&turn)) != 0)
		{
			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag0, false);
			MC2_enterLoop();
			while ((_mt=MC2_nextOpLoad(MCID_NODEP), t = load_32(&turn)) != 0)
			{
				// thrd_yield();
				;
			}
MC2_exitLoop();

			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag0, true);
			// std::atomic_thread_fence(std::memory_order_seq_cst);
		} else {
			;
			// thrd_yield();
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

void p1(void *arg)
{
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag1, true);
	// std::atomic_thread_fence(std::memory_order_seq_cst);

	MCID _mt; int f0, t;
	MC2_enterLoop();
	while (true)
	{
		MCID _mf0; _mf0=MC2_nextOpLoad(MCID_NODEP); int f0 = load_32(&flag0);
		MCID _br1;
		if (f0) {
			_br1 = MC2_branchUsesID(_mf0, 1, 2, true);
			;
			MC2_merge(_br1);
		} else {
			_br1 = MC2_branchUsesID(_mf0, 0, 2, true);
			break;
		}
		if ((_mt=MC2_nextOpLoad(MCID_NODEP), t = load_32(&turn)) != 1)
		{
			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag1, false);
			MC2_enterLoop();
			while ((_mt=MC2_nextOpLoad(MCID_NODEP), t = load_32(&turn)) != 1)
			{
				thrd_yield();
			}
MC2_exitLoop();

			MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
			store_32(&flag1, true);
			// std::atomic_thread_fence(std::memory_order_seq_cst);
		} else {
			;
			thrd_yield();
		}
	}
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

int user_main(int argc, char **argv)
{
	thrd_t a, b;

	flag0 = false;
	flag1 = false;
	turn = 0;

	thrd_create(&a, p0, NULL);
	thrd_create(&b, p1, NULL);

	thrd_join(a);
	thrd_join(b);

	return 0;
}

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
	store_32(&flag0, true);
	// std::atomic_thread_fence(std::memory_order_seq_cst);

	int f1;
	while (true)
	{
		f1 = load_32(&flag1);
		if (!f1) {
			break;
		}

		if (load_32(&turn)) {
			store_32(&flag0, false);
			while(1) {
				if (!load_32(&turn)) {
					break;
				}
				
				MC2_yield();
			}

			store_32(&flag0, true);
		} else {
			MC2_yield();
		}
	}

	// std::atomic_thread_fence(std::memory_order_acquire);

	// critical section
	store_32(&var, 1);

	store_32(&turn, 1);
	// std::atomic_thread_fence(std::memory_order_release);
	store_32(&flag0, false);
}

void p1() {
	store_32(&flag1, true);
	// std::atomic_thread_fence(std::memory_order_seq_cst);
	
	int f0;
	while (true) {
		int f0 = load_32(&flag0);
		if (!f0) {
			break;
		}
		
		if (!load_32(&turn)) {
			store_32(&flag1, false);
			while (1) {
				if (load_32(&turn))
					break;
				MC2_yield();
			}
			
			store_32(&flag1, true);
			// std::atomic_thread_fence(std::memory_order_seq_cst);
		}
	}

	// std::atomic_thread_fence(std::memory_order_acquire);

	// critical section
	store_32(&var, 2);

	store_32(&turn, 0);
	// std::atomic_thread_fence(std::memory_order_release);
	store_32(&flag1, false);
}

void p0l(void *a) {
	int i;
	for(i=0;i<PROBLEMSIZE;i++) {
		p0();
	}
}


void p1l(void *a) {
	int i;
	for(i=0;i<PROBLEMSIZE;i++) {
		p1();
	}
}


int user_main(int argc, char **argv)
{
	thrd_t a, b;

	store_32(&flag0, false);

	store_32(&flag1, false);

	store_32(&turn, 0);

	thrd_create(&a, p0l, NULL);
	thrd_create(&b, p1l, NULL);

	thrd_join(a);
	thrd_join(b);

	return 0;
}

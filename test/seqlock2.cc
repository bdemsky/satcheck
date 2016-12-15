#include <stdio.h>
#include <threads.h>

#include "libinterface.h"

typedef struct seqlock {
	// Sequence for reader consistency check
	int _seq;
	// It needs to be atomic to avoid data races
	int _data;

	seqlock() {
		MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
		store_32(&_seq, 0);
		MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
		store_32(&_data, 10);
	}

	int read() {
		int res;
		MC2_enterLoop();
		while (true) {
			//int old_seq = _seq.load(memory_order_acquire); // acquire
			MCID mold_seq = MC2_nextOpLoad(MCID_NODEP);
			int old_seq = load_32(&_seq);
			
			int cond0 = (old_seq % 2 == 1);
			MCID mcond0 = MC2_function(1, 4, cond0, mold_seq);
			MCID br0;

			if (cond0) {
				br0 = MC2_branchUsesID(mcond0, 1, 2, true);
				MC2_yield();
				MC2_merge(br0);
			} else {
				br0 = MC2_branchUsesID(mcond0, 0, 2, true);

				MCID mres = MC2_nextOpLoad(MCID_NODEP);
				res = load_32(&_data);

				MCID mseq = MC2_nextOpLoad(MCID_NODEP);
				int seq = load_32(&_seq); // _seq.load(memory_order_relaxed)

				int cond1 = seq == old_seq;
				MCID mcond1 = MC2_function(2, 4, cond1, mseq, mold_seq);
				MCID br1;
				if (cond1) { // relaxed
					br1 = MC2_branchUsesID(mcond1, 1, 2, true);
					break;
				} else {
					br1 = MC2_branchUsesID(mcond1, 0, 2, true);
					MC2_yield();
					MC2_merge(br1);
				}
				
				MC2_merge(br0);
			}
		}
		MC2_exitLoop();
		return res;
	}
	
	void write(int new_data) {
		MC2_enterLoop();
		while (true) {
			// This might be a relaxed too
			//int old_seq = _seq.load(memory_order_acquire); // acquire
			MCID mold_seq = MC2_nextOpLoad(MCID_NODEP);
			int old_seq = load_32(&_seq);

			int cond0 = (old_seq % 2 == 1);
			MCID mcond0 = MC2_function(1, 4, cond0, mold_seq);
			MCID br0;
			if (cond0) {
				//retry as the integer is odd
				br0 = MC2_branchUsesID(mcond0, 1, 2, true);
				MC2_yield();
				MC2_merge(br0);
			} else {
				br0 = MC2_branchUsesID(mcond0, 0, 2, true);

				int new_seq=old_seq+1;
				MCID mnew_seq=MC2_function(1, 4, new_seq, mold_seq);
				MCID m_priorval=MC2_nextRMW(MCID_NODEP, mold_seq, mnew_seq);
				int cas_value=rmw_32(CAS, &_seq, old_seq, new_seq);
				int exit=cas_value==old_seq;
				MCID m_exit=MC2_function(2, 4, exit, m_priorval, mold_seq);
								
				if (exit) {
					MCID br2=MC2_branchUsesID(m_exit, 1, 2, true);
					break;
				} else {
					MCID br2=MC2_branchUsesID(m_exit, 0, 2, true);
					MC2_yield();
					MC2_merge(br2);
				}
				//if cas fails, we have to retry as someone else succeeded
				MC2_merge(br0);
			}				
		}
		MC2_exitLoop();
		
		// Update the data
		//_data.store(new_data, memory_order_release); // release
		MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
		store_32(&_data, new_data);

		// _seq.fetch_add(1, memory_order_release); // release
		MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
		rmw_32(ADD, &_seq, /* dummy */0, 1);
	}

} seqlock_t;


seqlock_t *lock;

static void a(void *obj) {
	lock->write(30);
}

static void b(void *obj) {
	lock->write(20);
}

static void c(void *obj) {
	int r1 = lock->read();
}

int user_main(int argc, char **argv) {
	thrd_t t1, t2, t3;
	lock = new seqlock_t();

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	return 0;
}

#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "libinterface.h"

// Sequence for reader consistency check
/*atomic_*/ int _seq;
// It needs to be atomic to avoid data races
/*atomic_*/ int _data;

void seqlock_init() {
    MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
    store_32(&_seq, 0);
    MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
    store_32(&_data, 10);
}

int seqlock_read(MCID * retval) {
    MCID _mres; int res;
	MCID _mold_seq; _mold_seq=MC2_nextOpLoad(MCID_NODEP); int old_seq = load_32(&_seq); // acquire

	MCID _br30;
	
	int _cond30 = old_seq % 2 == 1;
	MCID _cond30_m = MC2_function_id(31, 1, sizeof(_cond30), _cond30, _mold_seq);
	if (_cond30) {
        _br30 = MC2_branchUsesID(_cond30_m, 1, 2, true);
        res = -1;
    	MC2_merge(_br30);
    } else {
		_br30 = MC2_branchUsesID(_cond30_m, 0, 2, true);
		_mres=MC2_nextOpLoad(MCID_NODEP), res = load_32(&_data);
        MCID _mseq; _mseq=MC2_nextOpLoad(MCID_NODEP); int seq = load_32(&_seq);
    
        MCID _br31;
        
        MCID _cond31_m;
        
        int _cond31 = MC2_equals(_mseq, (uint64_t)seq, _mold_seq, (uint64_t)old_seq, &_cond31_m);
        if (_cond31) {_br31 = MC2_branchUsesID(_cond31_m, 1, 2, true);
        
        	MC2_merge(_br31);
        } else {
            _br31 = MC2_branchUsesID(_cond31_m, 0, 2, true);
            res = -1;
        	MC2_merge(_br31);
        }
    	MC2_merge(_br30);
    }
    *retval = _mres;
    return res;
}
    
int seqlock_write(MCID _mnew_data, int new_data, MCID * retval) {
    MCID _mold_seq; _mold_seq=MC2_nextOpLoad(MCID_NODEP); int old_seq = load_32(&_seq);
    int res;
    
    MCID _br32;
    
    int _cond32 = old_seq % 2 == 1;
    MCID _cond32_m = MC2_function_id(32, 1, sizeof(_cond32), _cond32, _mold_seq);
    if (_cond32) {
        _br32 = MC2_branchUsesID(_cond32_m, 1, 2, true);
        res = 0;
    	MC2_merge(_br32);
    } else {
        MCID _fn0; _br32 = MC2_branchUsesID(_cond32_m, 0, 2, true);
        int new_seq = old_seq + 1;
        _fn0 = MC2_function_id(33, 1, sizeof (new_seq), (uint64_t)new_seq, _mold_seq); 
        MCID _rmw0 = MC2_nextRMW(MCID_NODEP, _mold_seq, _fn0);
        int cas_value = rmw_32(CAS, &_seq, old_seq, new_seq);
        
        MCID _br33;
        
        MCID _cond33_m;
        
        int _cond33 = MC2_equals(_rmw0, (uint64_t)cas_value, _mold_seq, (uint64_t)old_seq, &_cond33_m);
        if (_cond33) {
            // Update the data
            _br33 = MC2_branchUsesID(_cond33_m, 1, 2, true);
            MC2_nextOpStore(MCID_NODEP, _mnew_data);
            store_32(&_data, new_data);
			MCID _rmw1 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
			rmw_32(ADD, &_seq, /* dummy */0, 1);
            res = 1;
        	MC2_merge(_br33);
        } else {
            _br33 = MC2_branchUsesID(_cond33_m, 0, 2, true);
            res = 0;
        	MC2_merge(_br33);
        }
    	MC2_merge(_br32);
    }
    *retval = MCID_NODEP;
    return res;
}

static void a(void *obj) {
	int q;
    MC2_enterLoop();
    for (int i = 0; i < PROBLEMSIZE; i++) {
        MCID _rv0;
        q = seqlock_write(MCID_NODEP, 30, &_rv0);
	}
MC2_exitLoop();


}

static void b(void *obj) {
    int r1;
    MC2_enterLoop();
    for (int i = 0; i < PROBLEMSIZE; i++) {
        MCID _rv1;
        r1 = seqlock_read(&_rv1);
	}
MC2_exitLoop();


}

static void c(void *obj) {
    int r1;
    MC2_enterLoop();
    for (int i = 0; i < PROBLEMSIZE; i++) {
        MCID _rv2;
        r1 = seqlock_read(&_rv2);
	}
MC2_exitLoop();


}

int user_main(int argc, char **argv) {
    thrd_t t1; thrd_t t2; thrd_t t3;
    seqlock_init();

    thrd_create(&t1, (thrd_start_t)&a, NULL);
    thrd_create(&t2, (thrd_start_t)&b, NULL);
    thrd_create(&t3, (thrd_start_t)&c, NULL);

    thrd_join(t1);
    thrd_join(t2);
    thrd_join(t3);


    return 0;
}

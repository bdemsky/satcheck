#include <stdio.h>
#include <stdatomic.h>

#include "threads.h"
#include "libinterface.h"
#define PROBLEMSIZE 100

/*atomic_*/ int _seq;
/*atomic_*/ int _data;

void seqlock_init() {
    MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
    store_32(&_seq, 0);
    MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
    store_32(&_data, 10);
}

int seqlock_read(MCID * retval) {
    MCID _mres; int res;
    MCID _mold_seq; _mold_seq=MC2_nextOpLoad(MCID_NODEP); int old_seq = load_32(&_seq);

    MCID _br0;
    
    int _cond0 = old_seq % 2 == 1;
    MCID _cond0_m = MC2_function_id(1, 1, sizeof(_cond0), _cond0, _mold_seq);
    if (_cond0) {
        _br0 = MC2_branchUsesID(_cond0_m, 1, 2, true);
        res = -1;
        MC2_merge(_br0);
    } else {
        _br0 = MC2_branchUsesID(_cond0_m, 0, 2, true);
        _mres=MC2_nextOpLoad(MCID_NODEP), res = load_32(&_data);
        MCID _mseq; _mseq=MC2_nextOpLoad(MCID_NODEP); int seq = load_32(&_seq);
    
        MCID _br1;

        MCID _cond1_m;

        int _cond1 = MC2_equals(_mseq, seq, _mold_seq, old_seq, &_cond1_m);
        if (_cond1) {_br1 = MC2_branchUsesID(_cond1_m, 1, 2, true);
             // relaxed
            MC2_merge(_br1);
        } else {
            _br1 = MC2_branchUsesID(_cond1_m, 0, 2, true);
            res = -1;
            MC2_merge(_br1);
        }
        MC2_merge(_br0);
    }
    *retval = _mres;
    return res;
}
    
int seqlock_write(MCID _mnew_data, int new_data, MCID * retval) {
    MCID _mold_seq; _mold_seq=MC2_nextOpLoad(MCID_NODEP); int old_seq = load_32(&_seq);
    int res;
    MCID _br2;
    
    int _cond2 = old_seq % 2 == 1;
    MCID _cond2_m = MC2_function_id(2, 1, sizeof(_cond2), _cond2, _mold_seq);
    if (_cond2) {
        _br2 = MC2_branchUsesID(_cond2_m, 1, 2, true);
        res = 0;
        MC2_merge(_br2);
    } else {
        MCID _fn0; _br2 = MC2_branchUsesID(_cond2_m, 0, 2, true);
        int new_seq = old_seq + 1;
        _fn0 = MC2_function_id(3, 1, sizeof (new_seq), new_seq, _mold_seq); 
        MCID _rmw0 = MC2_nextRMW(MCID_NODEP, _mold_seq, _fn0);
        int cas_value = rmw_32(CAS, &_seq, old_seq, new_seq);
        MCID _br3;
        
        MCID _cond3_m;

        int _cond3 = MC2_equals(_rmw0, cas_value, _mold_seq, old_seq, &_cond3_m);
        if (_cond3) {
            _br3 = MC2_branchUsesID(_cond3_m, 1, 2, true);
            MC2_nextOpStore(MCID_NODEP, _mnew_data);
            store_32(&_data, new_data);
            MCID _rmw1 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
            rmw_32(ADD, &_seq, /*dummy */0, 1);
            res = 1;
            MC2_merge(_br3);
        } else {
            _br3 = MC2_branchUsesID(_cond3_m, 0, 2, true);
            res = 0;
            MC2_merge(_br3);
        }
        MC2_merge(_br2);
    }
    *retval = MCID_NODEP;
    return res;
}

static void a(void *obj) {
    MC2_enterLoop();
    for (int i = 0; i < PROBLEMSIZE; i++)
        seqlock_write(30);
MC2_exitLoop();

}

static void b(void *obj) {
    int r1;
    MC2_enterLoop();
    for (int i = 0; i < PROBLEMSIZE; i++)
        MCID _rv0;
        r1 = seqlock_read(&_rv0);
MC2_exitLoop();

}

static void c(void *obj) {
    int r1;
    MC2_enterLoop();
    for (int i = 0; i < PROBLEMSIZE; i++)
        MCID _rv1;
        r1 = seqlock_read(&_rv1);
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

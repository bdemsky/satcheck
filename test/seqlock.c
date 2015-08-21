#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include <stdatomic.h>
#include "threads.h"
#include "libinterface.h"

/*atomic_*/ int _seq;
/*atomic_*/ int _data;

void seqlock_init() {
    MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
    store_32(&_seq, 0);
    MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
    store_32(&_data, 0);
}

int seqlock_read() {
    MC2_enterLoop();
    while (true) {
        MCID _mold_seq; _mold_seq=MC2_nextOpLoad(MCID_NODEP); int old_seq = load_32(&_seq);

        MCID _fn0; int c0 = (old_seq % 2 == 1);
        _fn0 = MC2_function(1, sizeof (c0), c0, _mold_seq);
        MCID _br0;
        if (c0) {
            _br0 = MC2_branchUsesID(_fn0, 1, 2, true);
            MC2_yield();
            MC2_merge(_br0);
        } else {
            MCID _mres; _br0 = MC2_branchUsesID(_fn0, 0, 2, true); 
            _mres=MC2_nextOpLoad(MCID_NODEP); int res = load_32(&_data);
            MCID _mseq; _mseq=MC2_nextOpLoad(MCID_NODEP); int seq = load_32(&_seq);
            
            MCID _fn1; int c1;
            c1 = seq == old_seq;

            _fn1 = MC2_function(2, sizeof (c1), c1, _mold_seq, _mseq);              MCID _br1;
            if (c1) { // relaxed
                _br1 = MC2_branchUsesID(_fn1, 1, 2, true);
                MC2_exitLoop();
                MCID res_phi = MC2_loop_phi(_mres);
                return res;
            } else {
                _br1 = MC2_branchUsesID(_fn1, 0, 2, true);
                MC2_yield();
                MC2_merge(_br1);
            }
            MC2_merge(_br0);
        }
    }
    MC2_exitLoop();

}
    
void seqlock_write(MCID _mnew_data, int new_data) {
    MC2_enterLoop();
    while (true) {
        MCID _mold_seq; _mold_seq=MC2_nextOpLoad(MCID_NODEP); int old_seq = load_32(&_seq);
        MCID _fn2; int c2 = (old_seq % 2 == 1);
        _fn2 = MC2_function(1, sizeof (c2), c2, _mold_seq);
        MCID _br2;
        if (c2) {
            _br2 = MC2_branchUsesID(_fn2, 1, 2, true);
            MC2_yield();
            MC2_merge(_br2);
        } else {
            MCID _fn3; _br2 = MC2_branchUsesID(_fn2, 0, 2, true);
            int new_seq = old_seq + 1;
            _fn3 = MC2_function(1, sizeof (new_seq), new_seq, _mold_seq);
            MCID _rmw0 = MC2_nextRMW(MCID_NODEP, _mold_seq, _fn3);
            int cas_value = rmw_32(CAS, &_seq, old_seq, new_seq);
            MCID _fn4; int c3 = old_seq == cas_value;
            _fn4 = MC2_function(2, sizeof (c3), c3, _mold_seq, _rmw0);
            MCID _br3;
            if (c3) {
                _br3 = MC2_branchUsesID(_fn4, 1, 2, true);
                break;
            } else {
                _br3 = MC2_branchUsesID(_fn4, 0, 2, true);
                MC2_yield();
                MC2_merge(_br3);
            }
            MC2_merge(_br2);
        }
    }
    MC2_exitLoop();

    MC2_nextOpStore(MCID_NODEP, _mnew_data);
    store_32(&_data, new_data);

    MCID _rmw1 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
    rmw_32(ADD, &_seq, /*dummy */0, 1);
}

static void a(void *obj) {
    seqlock_write(MCID_NODEP, 3);
}

static void b(void *obj) {
    seqlock_write(MCID_NODEP, 2);
}

static void c(void *obj) {
    int r1 = seqlock_read();
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

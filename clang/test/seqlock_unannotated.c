#include <stdio.h>

#include "threads.h"
#include "libinterface.h"
#define PROBLEMSIZE 100

/*atomic_*/ int _seq;
/*atomic_*/ int _data;

void seqlock_init() {
    store_32(&_seq, 0);
    store_32(&_data, 10);
}

int seqlock_read() {
    int res;
    int old_seq = load_32(&_seq);

	if (old_seq % 2 == 1) {
		res = -1;
	} else {
        res = load_32(&_data);
        int seq = load_32(&_seq);
    
        if (seq == old_seq) { // relaxed
        } else {
            res = -1;
        }
    }
    return res;
}
    
int seqlock_write(int new_data) {
    int old_seq = load_32(&_seq);
    int res;
    if (old_seq % 2 == 1) {
        res = 0;
    } else {
        int new_seq = old_seq + 1;
        int cas_value = rmw_32(CAS, &_seq, old_seq, new_seq);
        if (cas_value == old_seq) {
            store_32(&_data, new_data);
            rmw_32(ADD, &_seq, /*dummy */0, 1);
            res = 1;
        } else {
            res = 0;
        }
    }
    return res;
}

static void a(void *obj) {
    for (int i = 0; i < PROBLEMSIZE; i++)
        seqlock_write(30);
}

static void b(void *obj) {
    int r1;
    for (int i = 0; i < PROBLEMSIZE; i++)
        r1 = seqlock_read();
}

static void c(void *obj) {
    int r1;
    for (int i = 0; i < PROBLEMSIZE; i++)
        r1 = seqlock_read();
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

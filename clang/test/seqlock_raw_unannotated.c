#include <stdio.h>
#include <threads.h>
#include "libinterface.h"

/*atomic_*/ int _seq;
/*atomic_*/ int _data;

void seqlock_init() {
	store_32(&_seq, 0);
	store_32(&_data, 0);
}

int seqlock_read() {
	while (true) {
		int old_seq = load_32(&_seq);

		if (old_seq % 2 == 1) {
		} else {
			int res = load_32(&_data);
			int seq = load_32(&_seq);
			
			if (seq == old_seq) { // relaxed
				return res;
			}
		}
	}
}
	
void seqlock_write(int new_data) {
	while (true) {
		int old_seq = load_32(&_seq);
		if (old_seq % 2 == 1) {
		} else {
			int new_seq = old_seq + 1;
			int cas_value = rmw_32(CAS, &_seq, old_seq, new_seq);
			if (old_seq == cas_value) {
				break;
			}
		}
	}
	store_32(&_data, new_data);

	rmw_32(ADD, &_seq, /*dummy */0, 1);
}

static void a(void *obj) {
	seqlock_write(3);
}

static void b(void *obj) {
	seqlock_write(2);
}

static void c(void *obj) {
	int r1 = seqlock_read();
}

int user_main(int argc, char **argv) {
	thrd_t t1, t2, t3;
	seqlock_init();

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	return 0;
}

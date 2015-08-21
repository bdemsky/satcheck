#include <threads.h>
#include <stdbool.h>
#include "libinterface.h"

/* atomic */ int flag1, flag2;

#define true 1
#define false 0
#define NULL 0

uint64_t var = 0, var2 = 0;

void p0() {
    load_64(&flag2);
    int q = load_64(&flag1);
    if (load_64(&flag1)) {
		;
	}
	if (!load_64(&flag1)) {
		;
	}
}

void p1() {
	rmw_64(CAS, &flag1, var, var2);
	int r = rmw_64(CAS, &flag1, var, var2);
	if (rmw_64(CAS, &flag1, var, var2)) {
		;
	}
}

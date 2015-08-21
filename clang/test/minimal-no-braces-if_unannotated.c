#include <threads.h>
#include <stdbool.h>
#include "libinterface.h"

/* atomic */ int flag1;

#define true 1
#define false 0
#define NULL 0

/* uint32_t */ int var = 0;

void p0() {
	int f1, t;
	while (true)
	{
		f1 = load_32(&flag1);
		if (!f1)
			break;
	}
}

void p1() {
	int f1, t;
	while (true)
	{
		f1 = load_32(&flag1);
		if (!f1)
			break;
		else
			;
	}
}

void p2() {
	int f1, t;
	while (true)
	{
		f1 = load_32(&flag1);
		if (!f1) {
			break;
		}
	}
}

void p3() {
	int f1, t;
	while (true)
	{
		f1 = load_32(&flag1);
		if (!f1) {
			break;
		} else {
			;
		}
	}
}


int user_main(int argc, char **argv)
{
	thrd_t a;

	store_32(&flag1, false);

	thrd_create(&a, p0, NULL);
	thrd_join(a);

	return 0;
}

void pf(int q) {
    // need to create a tmp variable etc
    if (q) {
		;
	}
}

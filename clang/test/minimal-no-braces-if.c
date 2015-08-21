#include <threads.h>
#include <stdbool.h>
#include "libinterface.h"

/* atomic */ int flag1;

#define true 1
#define false 0
#define NULL 0

/* uint32_t */ int var = 0;

void p0() {
	MCID _mf1; int f1, t;
	MC2_enterLoop();
	while (true)
	{
		_mf1=MC2_nextOpLoad(MCID_NODEP), f1 = load_32(&flag1);
		MCID _br0;
		if (!f1)
			{_br0 = MC2_branchUsesID(_mf1, 1, 2, true);
			break;
}	 else { _br0 = MC2_branchUsesID(_mf1, 0, 2, true);	MC2_merge(_br0);
	 }}
MC2_exitLoop();

}

void p1() {
	MCID _mf1; int f1, t;
	MC2_enterLoop();
	while (true)
	{
		_mf1=MC2_nextOpLoad(MCID_NODEP), f1 = load_32(&flag1);
		MCID _br1;
		if (!f1)
			{_br1 = MC2_branchUsesID(_mf1, 1, 2, true);
			break;
}		else
			{_br1 = MC2_branchUsesID(_mf1, 0, 2, true);
				MC2_merge(_br1);
			;}
	}
MC2_exitLoop();

}

void p2() {
	MCID _mf1; int f1, t;
	MC2_enterLoop();
	while (true)
	{
		_mf1=MC2_nextOpLoad(MCID_NODEP), f1 = load_32(&flag1);
		MCID _br2;
		if (!f1) {
			_br2 = MC2_branchUsesID(_mf1, 1, 2, true);
			break;
		}
	} else { _br2 = MC2_branchUsesID(_mf1, 0, 2, true);	MC2_merge(_br2);
	 }
MC2_exitLoop();

}

void p3() {
	MCID _mf1; int f1, t;
	MC2_enterLoop();
	while (true)
	{
		_mf1=MC2_nextOpLoad(MCID_NODEP), f1 = load_32(&flag1);
		MCID _br3;
		if (!f1) {
			_br3 = MC2_branchUsesID(_mf1, 1, 2, true);
			break;
		} else {
			_br3 = MC2_branchUsesID(_mf1, 0, 2, true);
			;
			MC2_merge(_br3);
		}
	}
MC2_exitLoop();

}


int user_main(int argc, char **argv)
{
	thrd_t a;

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&flag1, false);

	thrd_create(&a, p0, NULL);
	thrd_join(a);

	return 0;
}

void pf(MCID _mq, int q) {
    // need to create a tmp variable etc
    MCID _br4;
    if (q) {
		_br4 = MC2_branchUsesID(, 1, 2, true);
		;
		MC2_merge(_br4);
	}
 else { _br4 = MC2_branchUsesID(, 0, 2, true);	MC2_merge(_br4);
 }}

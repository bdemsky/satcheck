#include <stdio.h>
#include <threads.h>

#include "libinterface.h"

int x;
int y;
int gr1;
int gr2;

static void a(void *obj)
{
	MCID mr1=MC2_nextOpLoad(MCID_NODEP);
	int r1=load_32(&y);
	MCID br1;
	if (r1) {
		br1=MC2_branchUsesID(mr1, 1, 2, true);
		r1=42;
		mr1=MC2_function(0, 4, r1);  
	} else {
		br1=MC2_branchUsesID(mr1, 0, 2, true);
		r1=0;
		mr1=MC2_function(0, 4, r1);  
	}
	MC2_merge(br1);
	mr1=MC2_phi(mr1);

	MC2_nextOpStore(MCID_NODEP, mr1);
	store_32(&x, r1);

	MC2_nextOpStore(MCID_NODEP, mr1);
	store_32(&gr1, r1);
	printf("r1=%d\n",r1);
}

static void b(void *obj)
{
	MCID mr2=MC2_nextOpLoad(MCID_NODEP);
	int r2=load_32(&x);
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&y, 42);

	MC2_nextOpStore(MCID_NODEP, mr2);
	store_32(&gr2, r2);
	printf("r2=%d\n",r2);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&x, 0);
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&y, 0);

	printf("Main thread: creating 2 threads\n");
	MC2_nextOpThrd_create(MCID_NODEP, MCID_NODEP);
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	MC2_nextOpThrd_create(MCID_NODEP, MCID_NODEP);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	MC2_nextOpThrd_join(MCID_NODEP);
	thrd_join(t1);
	MC2_nextOpThrd_join(MCID_NODEP);
	thrd_join(t2);
	printf("Main thread is finished\n");
	MCID mr1=MC2_nextOpLoad(MCID_NODEP);
	int lgr1=load_32(&gr1);

	MCID mr2=MC2_nextOpLoad(MCID_NODEP);
	int lgr2=load_32(&gr2);
	MCID br1,br2;
	if (lgr1==42) {
		br1=MC2_branchUsesID(mr1, 1, 2, true);
		if (lgr2==42) {
			br2=MC2_branchUsesID(mr2, 1, 2, true);
		} else {
			br2=MC2_branchUsesID(mr2, 0, 2, true);
		}
		MC2_merge(br2);
	} else {
		br1=MC2_branchUsesID(mr1, 0, 2, true);
	}
	MC2_merge(br1);

	return 0;
}

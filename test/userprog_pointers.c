#include <stdio.h>
#include "threads.h"
#include "libinterface.h"

int x;
int y;
int z;
int *px;
int *py;
int gr1;
int gr2;

int user_main(int argc, char **argv)
{
	//	thrd_t t1, t2;

	// MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&x, 0);   // x = 0
	// MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&y, 0);   // y = 0

	// MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&py, (uint32_t)&z); // py = &z
	// MCID mpx = MC2_nextOpLoad(MCID_NODEP);
	px = (int *)load_32(&py); // px = y;
	// MC2_nextOpStore(mpx, MCID_NODEP);
	store_32(px, 25); // *px = 25;
	
	// MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&px, (uint32_t)&x); // px = &x

	// MC2_nextOpStore(MCID_NODEP, mx);
	store_32(&y, x);
	// MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&y, (uint32_t) &x);

	// MC2_nextOpStore(MCID_NODEP, mpx);
	store_32(&y, (uint32_t) px);
	// MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(&y, (uint32_t) &px);

	return 0;
}

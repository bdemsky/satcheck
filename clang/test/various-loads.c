#include <threads.h>
#include <stdbool.h>
#include "libinterface.h"

/* atomic */ int flag1, flag2;

#define true 1
#define false 0
#define NULL 0

uint64_t var = 0, var2 = 0;

void p0() {MC2_nextOpLoad(MCID_NODEP); 
    load_64(&flag2);
    MCID _mq; _mq=MC2_nextOpLoad(MCID_NODEP); int q = load_64(&flag1);
    MCID _br0;
    MCID _m_cond0_m=MC2_nextOpLoad(MCID_NODEP); 
    int _cond0 = load_64(&flag1);
    MCID _cond0_m = MC2_function_id(1, 1, sizeof(_cond0), _cond0);
    if (_cond0) {
		_br0 = MC2_branchUsesID(_cond0_m, 1, 2, true);
		;
		MC2_merge(_br0);
	}
 else { _br0 = MC2_branchUsesID(_cond0_m, 0, 2, true);	MC2_merge(_br0);
	 }	MCID _br1;
	MC2_nextOpLoad(MCID_NODEP); 
	int _cond1 = !load_64(&flag1);
	MCID _cond1_m = MC2_function_id(2, 1, sizeof(_cond1), _cond1);
	if (_cond1) {
		_br1 = MC2_branchUsesID(_cond1_m, 0, 2, true);
		;
		MC2_merge(_br1);
	}
 else { _br1 = MC2_branchUsesID(_cond1_m, 1, 2, true);	MC2_merge(_br1);
 }}

void p1() {
	MCID _rmw0 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
	rmw_64(CAS, &flag1, var, var2);
	MCID _rmw1 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
	int r = rmw_64(CAS, &flag1, var, var2);
	MCID _br2;
	MCID _rmw2 = MC2_nextRMW(MCID_NODEP, MCID_NODEP, MCID_NODEP);
	
	int _cond2 = rmw_64(CAS, &flag1, var, var2);
	MCID _cond2_m = MC2_function_id(3, 1, sizeof(_cond2), _cond2);
	if (_cond2) {
		_br2 = MC2_branchUsesID(_cond2_m, 1, 2, true);
		;
		MC2_merge(_br2);
	}
 else { _br2 = MC2_branchUsesID(_cond2_m, 0, 2, true);	MC2_merge(_br2);
 }}

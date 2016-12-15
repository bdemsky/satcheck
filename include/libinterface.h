/** @file libinterface.h
 *  @brief Interface to check normal memory operations for data races.
 */

#ifndef __LIBINTERFACE_H__
#define __LIBINTERFACE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
	typedef unsigned int MCID;
#define MC2_PTR_LENGTH sizeof(void *)

#define MCID_NODEP ((MCID)0)
#define MCID_INIT ((MCID)0)
#define MCID_FIRST ((MCID)1)

#define MC2_OFFSET(x, y) (uintptr_t)(&((x)(0))->y)

	/** Stores N bits of val to the location given by addr. */
	void store_8(void *addr, uint8_t val);
	void store_16(void *addr, uint16_t val);
	void store_32(void *addr, uint32_t val);
	void store_64(void *addr, uint64_t val);

	/** Loads N bits from the location given by addr. */
	uint8_t load_8(const void *addr);
	uint16_t load_16(const void *addr);
	uint32_t load_32(const void *addr);
	uint64_t load_64(const void *addr);

	/** Atomic operation enumeration for RMW operations. */
	
	enum atomicop {
		ADD,
		CAS /* Compare and swap */,
		EXC /* Exchange */
	};

	/** Performs an atomic N bit RMW (read-modify-write) operation as
	 *	specified by op to data located at addr.
	 *  For CAS, valarg is the value to be written, while
	 *						oldval gives the value to be compared against.
	 *	For EXC, valarg is the value to be written and oldval is ignored.
	 *	For ADD, valarg is the value to be added and oldval is ignored. */
	
	uint8_t rmw_8(enum atomicop op, void *addr, uint8_t oldval, uint8_t valarg);
	uint16_t rmw_16(enum atomicop op, void *addr, uint16_t oldval, uint16_t valarg);
	uint32_t rmw_32(enum atomicop op, void *addr, uint32_t oldval, uint32_t valarg);
	uint64_t rmw_64(enum atomicop op, void *addr, uint64_t oldval, uint64_t valarg);

	/** Supplies to MC2 the MCIDs for the addrs of the
	 *	immediately-following RMW/Load/Store operation. All
	 *	RMW/Load/Store operations must be immediately preceded by either
	 *	a MC2_nextRMW/MC2_nextOpLoad/MC2_nextOpStore or one of the
	 *	offset calls specified in the next section. For MC2_nextRMW and
	 *	MC2_nextOpLoad, MC2 returns an MCID for the value read by the
	 *	RMW/load operation. */
	
	MCID MC2_nextRMW(MCID addr, MCID oldval, MCID valarg);
	MCID MC2_nextOpLoad(MCID addr);
	void MC2_nextOpStore(MCID addr, MCID value);
	
	/** Supplies to MC2 the MCIDs representing the target of the
	 *	immediately-following RMW/Load/Store operation, which accesses
	 *	memory at a fixed offset from addr.
	 * 
	 *	e.g. load_8(x.f) could be instrumented with
	 *	MC2_nextOpLoadOffset(_m_x, offset of f in structure x). */
		
	MCID MC2_nextRMWOffset(MCID addr, uintptr_t offset, MCID oldval, MCID valarg);
	MCID MC2_nextOpLoadOffset(MCID addr, uintptr_t offset);
	void MC2_nextOpStoreOffset(MCID addr, uintptr_t offset, MCID value);

	/** Tells MC2 that we just took direction "direction" of a
	 *	conditional branch with "num_directions" possible directions.
	 *	"condition" gives the MCID for the concrete condition variable
	 *	that we conditionally branched on. Boolean anyvalue = true means
	 *	that any non-zero concrete condition implies the branch will
	 *	take direction 1. */

	MCID MC2_branchUsesID(MCID condition, int direction, int num_directions, bool anyvalue);

	/** Currently not used. TODO implement later. */
	void MC2_nextOpThrd_create(MCID startfunc, MCID param);
	void MC2_nextOpThrd_join(MCID jointhrd);

	/** Tells MC2 that we hit the merge point of a conditional
	 *	branch. branchid is the MCID returned by the corresponding
	 *	MC2_branchUsesID function for the branch that just merged. */
	void MC2_merge(MCID branchid);

	/** Tells MC2 that we just computed something that MC2 should treat
	 *	as an uninterpreted function. The uninterpreted function takes
	 *	num_args parameters and produces a return value of
	 *	numbytesretval bytes; the actual return value is val.  The
	 *	following varargs parameters supply MCIDs for inputs.  MC2
	 *	returns an MCID for the return value of the uninterpreted
	 *	function.
	 *
	 *	Uninterpreted functions annotated with MC2_function are unique
	 *	to the given dynamic instance. That is, for an unintepreted
	 *	function call annotated by MC2_function which lives inside a
	 *	loop, results from the first iteration will NOT be aggregated
	 *	with results from subsequent iterations. */
	MCID MC2_function(unsigned int num_args, int numbytesretval, uint64_t val, ...);

	/** Same as MC2_function, but MC2 aggregates results for all
	 * 	instances with the same id. The id must be greater than 0.
	 * 	Useful for functions which have MCIDs for all inputs. 
	 *	(MC2_function, by contrast, may omit MCIDs for some inputs.) */
	MCID MC2_function_id(unsigned int id, unsigned int num_args, int numbytesretval, uint64_t val, ...);

	/** Asks MC2 to compare val1 (with MCID op1) and val2 (MCID op2);
	 * 	returns val1==val2, and sets *retval to the MCID for the return value. */
	// This function's API should change to take only op1 and op2, and return the MCID for the retval. This is difficult with the old frontend.
	uint64_t MC2_equals(MCID op1, uint64_t val1, MCID op2, uint64_t val2, MCID *retval);
	
	/** Tells MC2 about the MCID of an input which was merged at a merge
	 *  point. (The merge point must also be annotated, first, with an
	 *  MC2_merge). Returns an MCID for the output of the phi function.
	 *
	 * 	MC2_phi enables instrumenting cases like:
	 *	if (x) {
	 *		y=0;
	 *	} else {
	 *		y=1;
	 *	}
	 *	// must put here: MC2_merge(_branch_id);
	 *	// 								MC2_phi(_m_y);
	*/

	MCID MC2_phi(MCID input);

	/** Tells MC2 about the MCID for an input used within a loop.
	 *
	 *	Example:
	 *
	 *	while (...) {
	 *		x=x+1;
	 *		if (...)
	 *			break;
	 *	}
	 *	// MC2_exitLoop(...);
	 *	// MC2_loop_phi(_m_x);
	 *
	 */

	MCID MC2_loop_phi(MCID input);

	/** Tells MC2 about a yield. (Placed by programmers.) */
	void MC2_yield();
	
	/** Tells MC2 that the next operation is a memory fence. */
	void MC2_fence();

	/** Tells MC2 that the next statement is the head of a loop. */
	void MC2_enterLoop();

	/** Tells MC2 that the previous statement exits a loop. */
	void MC2_exitLoop();

	/** Tells MC2 that a loop is starting a new iteration. 
	 *	Not currently strictly required. */
	void MC2_loopIterate();
#ifdef __cplusplus
}
#endif

#endif /* __LIBINTERFACE_H__ */

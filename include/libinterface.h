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
			specified by op to the address specified by addr.  
			For CAS, oldval gives the value to be compared against while
			valarg is the value to be written.
			For EXC, valarg is the value to be written and oldval is ignored.
			For ADD, valarg is the value to be added and oldval is ignored. */
	
	uint8_t rmw_8(enum atomicop op, void *addr, uint8_t oldval, uint8_t valarg);
	uint16_t rmw_16(enum atomicop op, void *addr, uint16_t oldval, uint16_t valarg);
	uint32_t rmw_32(enum atomicop op, void *addr, uint32_t oldval, uint32_t valarg);
	uint64_t rmw_64(enum atomicop op, void *addr, uint64_t oldval, uint64_t valarg);


	/** Specifies the sources of the arguments for the following
			RMW/Load/Store operation.  All RMW/Load/Store operations must be
			immediately preceded by a
			MC2_nextRMW/MC2_nextOpLoad/MC2_nextOpStore call or one of the
			offset calls specified in the next section below. The
			MC2_nextRMW and MC2_nextOpLoad return a MCID for the value
			read.*/
	
	MCID MC2_nextRMW(MCID addr, MCID oldval, MCID valarg);
  MCID MC2_nextOpLoad(MCID addr);
  void MC2_nextOpStore(MCID addr, MCID value);
	
	/** Specifies the sources of the arguments for the following
			RMW/Load/Store operation that includes a fixed offset from the
			address specified by addr. 
			For example, load_8(x.f) could be be instrumented with a
			MC2_nextOpLoadOffset(_m_x, offset of f in structure). */
		
	MCID MC2_nextRMWOffset(MCID addr, uintptr_t offset, MCID oldval, MCID valarg);
  MCID MC2_nextOpLoadOffset(MCID addr, uintptr_t offset);
  void MC2_nextOpStoreOffset(MCID addr, uintptr_t offset, MCID value);

	/** Specifies that we just took the direction "direction" of a
			conditional branch with "num_directions" possible directions.
			The MCIDcondition gives the MCID for the variable that we
			conditionally branched on.  The boolean anyvalue is set if any
			non-zero value means that the branch will simply take direction
			1.
	*/

	MCID MC2_branchUsesID(MCID condition, int direction, int num_directions, bool anyvalue);

	/** Currently not used.  Should be implemented later.  */
	void MC2_nextOpThrd_create(MCID startfunc, MCID param);
  void MC2_nextOpThrd_join(MCID jointhrd);

	/** Specifies that we have hit the merge point of a conditional
			branch.  The MCID branchid specifies the conditional branch that
			just merged.  */

	
  void MC2_merge(MCID branchid);

	/** Specifies a uninterpreted function with num_args parameters, a
			return value of numbytesretval bytes that returned the actual
			value val.  The MCIDs for all inputs are then specified
			afterwards.  The return MCID is MCID corresponding to the output
			of the uninterpreted function.  Uninterpreted functions
			specified via this annotation are unique to the given dynamic
			instance.  For example, if there is an unintepreted function
			call in a loop annotated by MC2_function, results from the first
			iteration will NOT be aggregated with results from later
			iterations.
	*/

	MCID MC2_function(unsigned int num_args, int numbytesretval, uint64_t val, ...);

	/** Arguments are the same as MC2_function, but results are
			aggregated for all instances with the same id.  The id must be
			greater than 0. This is generally useful for functions where all
			inputs are specified via MCIDs. */

  MCID MC2_function_id(unsigned int id, unsigned int num_args, int numbytesretval, uint64_t val, ...);

	/** MC2_equals implements equality comparison.  It takes in two
			arguments (val1 and val2) and the corresponding MCIDs (op1 and
			op2).  It returns the result val1==val2.  The MCID for the
			return value is returned via the pointer *retval.  */

	uint64_t MC2_equals(MCID op1, uint64_t val1, MCID op2, uint64_t val2, MCID *retval);
	
	/** MC2_phi specifies a phi function.  The MCID input is the input
			identifier and the returned MCID is the output of the phi
			function.  This is generally useful for instrumenting cases of the following form:
			if (x) {
			   y=0;
			} else {
			   y=1;
			}
			This example would require a MC2_phi at the merge point.
	*/

	MCID MC2_phi(MCID input);

	/** MC2_loop_phi is a phi function for loops. This is useful for code like the following:
			while() {
          x=x+1;
					if (....)
					  break;
			}
			return x;
			This example would require a MC2_loop_phi after the MC2_exitLoop annotation for the loop.
	 */

	MCID MC2_loop_phi(MCID input);

	/** MC2_yield is a yield operation. */
  void MC2_yield();
	
	/** MC2_fence specifies that the next operation is a memory fence. */
	void MC2_fence();

	/** MC2_enterLoop specifies that the next statement is a loop. */
	void MC2_enterLoop();

	/** MC2_exitLoop specifies exit of a loop. */
	void MC2_exitLoop();

	/** MC2_loopIterator specifies the next iteration of a loop. This is
			currently not strictly necessary to use. */
  void MC2_loopIterate();
#ifdef __cplusplus
}
#endif

#endif /* __LIBINTERFACE_H__ */

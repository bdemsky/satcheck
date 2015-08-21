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
	
	void store_8(void *addr, uint8_t val);
	void store_16(void *addr, uint16_t val);
	void store_32(void *addr, uint32_t val);
	void store_64(void *addr, uint64_t val);

	uint8_t load_8(const void *addr);
	uint16_t load_16(const void *addr);
	uint32_t load_32(const void *addr);
	uint64_t load_64(const void *addr);
  
	enum atomicop {
		ADD,
		CAS,
		EXC
	};

	uint8_t rmw_8(enum atomicop op, void *addr, uint8_t oldval, uint8_t valarg);
	uint16_t rmw_16(enum atomicop op, void *addr, uint16_t oldval, uint16_t valarg);
	uint32_t rmw_32(enum atomicop op, void *addr, uint32_t oldval, uint32_t valarg);
	uint64_t rmw_64(enum atomicop op, void *addr, uint64_t oldval, uint64_t valarg);
	MCID MC2_nextRMW(MCID addr, MCID oldval, MCID valarg);

  MCID MC2_nextOpLoad(MCID addr);
  void MC2_nextOpStore(MCID addr, MCID value);

	MCID MC2_nextRMWOffset(MCID addr, uintptr_t offset, MCID oldval, MCID valarg);

  MCID MC2_nextOpLoadOffset(MCID addr, uintptr_t offset);
  void MC2_nextOpStoreOffset(MCID addr, uintptr_t offset, MCID value);
  MCID MC2_branchUsesID(MCID condition, int direction, int num_directions, bool anyvalue);
  void MC2_nextOpThrd_create(MCID startfunc, MCID param);
  void MC2_nextOpThrd_join(MCID jointhrd);

  void MC2_merge(MCID branchid);
  MCID MC2_function(unsigned int num_args, int numbytesretval, uint64_t val, ...);
	MCID MC2_function_id(unsigned int id, unsigned int num_args, int numbytesretval, uint64_t val, ...);
	uint64_t MC2_equals(MCID op1, uint64_t val1, MCID op2, uint64_t val2, MCID *retval);
	
  MCID MC2_phi(MCID input);
	MCID MC2_loop_phi(MCID input);

  void MC2_yield();
	void MC2_fence();
  void MC2_enterLoop();
  void MC2_exitLoop();
  void MC2_loopIterate();
#ifdef __cplusplus
}
#endif

#endif /* __LIBINTERFACE_H__ */

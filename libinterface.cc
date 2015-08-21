/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdarg.h>

#include "libinterface.h"
#include "common.h"
#include "model.h"
#include "threads-model.h"
#include "mcexecution.h"

void store_8(void *addr, uint8_t val)
{
	DEBUG("addr = %p, val = %" PRIu8 "\n", addr, val);
	model->get_execution()->store(addr, val, sizeof(val));
}

void store_16(void *addr, uint16_t val)
{
	DEBUG("addr = %p, val = %" PRIu16 "\n", addr, val);
	model->get_execution()->store(addr, val, sizeof(val));
}

void store_32(void *addr, uint32_t val)
{
	DEBUG("addr = %p, val = %" PRIu32 "\n", addr, val);
	model->get_execution()->store(addr, val, sizeof(val));
}

void store_64(void *addr, uint64_t val)
{
	DEBUG("addr = %p, val = %" PRIu64 "\n", addr, val);
	model->get_execution()->store(addr, val, sizeof(val));
}

uint8_t load_8(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	return (uint8_t) model->get_execution()->load(addr, sizeof(uint8_t));
}

uint16_t load_16(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	return (uint16_t) model->get_execution()->load(addr,  sizeof(uint16_t));
}

uint32_t load_32(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	return (uint32_t) model->get_execution()->load(addr, sizeof(uint32_t));
}

uint64_t load_64(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	return (uint64_t) model->get_execution()->load(addr,  sizeof(uint64_t));
}

uint8_t rmw_8(enum atomicop op, void *addr, uint8_t oldval, uint8_t valarg) {
	return (uint8_t) model->get_execution()->rmw(op, addr, sizeof(oldval), (uint64_t) *((uint8_t *)addr), (uint64_t) oldval, (uint64_t) valarg);
}

uint16_t rmw_16(enum atomicop op, void *addr, uint16_t oldval, uint16_t valarg) {
	return (uint16_t) model->get_execution()->rmw(op, addr, sizeof(oldval), (uint64_t) *((uint16_t *)addr), (uint64_t) oldval, (uint64_t) valarg);
}

uint32_t rmw_32(enum atomicop op, void *addr, uint32_t oldval, uint32_t valarg) {
	return (uint32_t) model->get_execution()->rmw(op, addr, sizeof(oldval), (uint64_t) *((uint32_t *)addr), (uint64_t) oldval, (uint64_t) valarg);
}

uint64_t rmw_64(enum atomicop op, void *addr, uint64_t oldval, uint64_t valarg) {
	return model->get_execution()->rmw(op, addr, sizeof(oldval), *((uint64_t *)addr), oldval, valarg);
}

void MC2_nextOpThrd_create(MCID startfunc, MCID param) {
}

void MC2_nextOpThrd_join(MCID jointhrd) {
}

MCID MC2_nextOpLoad(MCID addr) {
	return model->get_execution()->loadMCID(addr, 0);
}

void MC2_nextOpStore(MCID addr, MCID value) {
	return model->get_execution()->storeMCID(addr, 0, value);
}

MCID MC2_nextOpLoadOffset(MCID addr, uintptr_t offset) {
	return model->get_execution()->loadMCID(addr, offset);
}

void MC2_nextOpStoreOffset(MCID addr, uintptr_t offset, MCID value) {
	return model->get_execution()->storeMCID(addr, offset, value);
}

MCID MC2_branchUsesID(MCID condition, int direction, int num_directions, bool anyvalue) {
	return model->get_execution()->branchDir(condition, direction, num_directions, anyvalue);
}

void MC2_merge(MCID branchid) {
	model->get_execution()->merge(branchid);
}

MCID MC2_phi(MCID input) {
	return model->get_execution()->phi(input);
}

MCID MC2_loop_phi(MCID input) {
	return model->get_execution()->loop_phi(input);
}

uint64_t MC2_equals(MCID op1, uint64_t val1, MCID op2, uint64_t val2, MCID *retval) {
	return model->get_execution()->equals(op1, val1, op2, val2, retval);
}


MCID MC2_function(uint num_args, int numbytesretval, uint64_t val, ...) {
	va_list vl;
	MCID mcids[num_args];
	va_start(vl, val);
	uint num_real_args=0;;
	for(uint i=0;i<num_args;i++) {
		MCID v=va_arg(vl, MCID);
		if (v!=MCID_NODEP)
			mcids[num_real_args++]=v;
	}
	va_end(vl);
	MCID m=model->get_execution()->function(0, numbytesretval, val, num_real_args, mcids);
	return m;
}

MCID MC2_function_id(unsigned int id, uint num_args, int numbytesretval, uint64_t val, ...) {
	va_list vl;
	MCID mcids[num_args];
	va_start(vl, val);
	uint num_real_args=0;;
	for(uint i=0;i<num_args;i++) {
		MCID v=va_arg(vl, MCID);
		if (v!=MCID_NODEP)
			mcids[num_real_args++]=v;
	}
	va_end(vl);
	MCID m=model->get_execution()->function(id, numbytesretval, val, num_real_args, mcids);
	return m;
}

void MC2_enterLoop() {
	model->get_execution()->enterLoop();
}

void MC2_exitLoop() {
	model->get_execution()->exitLoop();
}

void MC2_loopIterate() {
	model->get_execution()->loopIterate();
}

void MC2_yield() {
	model->get_execution()->threadYield();
}

void MC2_fence() {
#ifdef TSO
	model->get_execution()->fence();
#endif
}


MCID MC2_nextRMW(MCID addr, MCID oldval, MCID valarg) {
	return model->get_execution()->nextRMW(addr, 0, oldval, valarg);
}

MCID MC2_nextRMWOffset(MCID addr, uintptr_t offset, MCID oldval, MCID valarg) {
	return model->get_execution()->nextRMW(addr, offset, oldval, valarg);
}

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef MC_UTIL_H
#define MC_UTIL_H
#include <stdint.h>

static inline unsigned int rotate_left(unsigned int val, int bytestorotate) {
	return (val << bytestorotate) | (val >> (sizeof(val)*8-bytestorotate));
}

static inline unsigned int rotate_right(unsigned int val, int bytestorotate) {
	return (val >> bytestorotate) | (val << (sizeof(val)*8-bytestorotate));
}

static inline uint64_t keepbytes(uint64_t val, int len) {
	int shift=8-len;
	return (val << shift)>>shift;
}

static inline uint64_t getmask(uint len) {
	return 0xffffffffffffffff>>(64-(len*8));
}


#define NUMBITS(x) ((x==0)?0:8*sizeof(x)-__builtin_clz(x))

#endif

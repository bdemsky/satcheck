/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/**
 * @file context.h
 * @brief ucontext header, since Mac OSX swapcontext() is broken
 */

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ucontext.h>

#ifdef MAC

int model_swapcontext(ucontext_t *oucp, ucontext_t *ucp);

#else	/* !MAC */

static inline int model_swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	return swapcontext(oucp, ucp);
}

#endif/* !MAC */

#endif/* __CONTEXT_H__ */

/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/** @file config.h
 * @brief Configuration file.
 */

#ifndef CONFIG_H
#define CONFIG_H

/** Turn on debugging. */
#ifndef CONFIG_DEBUG
//#define CONFIG_DEBUG
#endif

#ifndef CONFIG_ASSERT
//#define CONFIG_ASSERT
#endif

//#define VERBOSE_CONSTRAINTS

/** Turn on support for dumping cyclegraphs as dot files at each
 *  printed summary.*/
//#define SUPPORT_MOD_ORDER_DUMP 1

/** Snapshotting configurables */

/**
 * If USE_MPROTECT_SNAPSHOT=2, then snapshot by tuned mmap() algorithm
 * If USE_MPROTECT_SNAPSHOT=1, then snapshot by using mmap() and mprotect()
 * If USE_MPROTECT_SNAPSHOT=0, then snapshot by using fork() */
#define USE_MPROTECT_SNAPSHOT 2

/** Size of signal stack */
#define SIGSTACKSIZE 65536

/** Page size configuration */
#define PAGESIZE 4096

/** Thread parameters */

/* Size of stack to allocate for a thread. */
#define STACK_SIZE (1024 * 1024)

/** Enable debugging assertions (via ASSERT()) */
//#define CONFIG_ASSERT

/** Enable dumping event graphs in DOT compatiable format. */
//#define DUMP_EVENT_GRAPHS

/** Print Achieved Goals. */
//#define PRINT_ACHIEVED_GOALS

/** Use TSO Memory Model */
//#define TSO

/** Record Stats */
//#define STATS

#endif

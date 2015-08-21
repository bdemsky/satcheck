/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/** @file snapshot.h
 *	@brief Snapshotting interface header file.
 */

#ifndef _SNAPSHOT_H
#define _SNAPSHOT_H

#include "snapshot-interface.h"
#include "config.h"
#include "mymemory.h"

void snapshot_add_memory_region(void *ptr, unsigned int numPages);
snapshot_id take_snapshot();
void snapshot_roll_back(snapshot_id theSnapShot);

#if !USE_MPROTECT_SNAPSHOT
mspace create_shared_mspace();
#endif

#endif

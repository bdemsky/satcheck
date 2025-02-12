/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

/**
 * @file snapshot-interface.h
 * @brief C interface layer on top of snapshotting system
 */

#ifndef __SNAPINTERFACE_H
#define __SNAPINTERFACE_H

typedef unsigned int snapshot_id;

typedef void (*VoidFuncPtr)();
void snapshot_system_init(unsigned int numbackingpages,
													unsigned int numsnapshots, unsigned int nummemoryregions,
													unsigned int numheappages, VoidFuncPtr entryPoint);

void snapshot_stack_init();
void snapshot_record(int seq_index);
int snapshot_backtrack_before(int seq_index);

#endif

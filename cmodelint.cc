/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#include "model.h"
#include "cmodelint.h"
#include "threads-model.h"

/** Performs a read action.*/
uint64_t model_read_action(void * obj, memory_order ord) {
  return -1;
}

/** Performs a write action.*/
void model_write_action(void * obj, memory_order ord, uint64_t val) {
}

/** Performs an init action. */
void model_init_action(void * obj, uint64_t val) {
}

/**
 * Performs the read part of a RMW action. The next action must either be the
 * write part of the RMW action or an explicit close out of the RMW action w/o
 * a write.
 */
uint64_t model_rmwr_action(void *obj, memory_order ord) {
  return -1;
}

/** Performs the write part of a RMW action. */
void model_rmw_action(void *obj, memory_order ord, uint64_t val) {
}

/** Closes out a RMW action without doing a write. */
void model_rmwc_action(void *obj, memory_order ord) {
}

/** Issues a fence operation. */
void model_fence_action(memory_order ord) {
}

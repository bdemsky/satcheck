/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef CLASSLIST_H
#define CLASSLIST_H
#include "hashset.h"
#include "mymemory.h"
#include <inttypes.h>

class StoreLoadSet;
class RecPair;
class MCChange;
class CGoal;
class EPRecord;
class EPValue;
class EventRecord;
class ExecPoint;
class MC;
class MCExecution;
class MCScheduler;
class ModelAction;
class Planner;
class Thread;
class ConstGen;
class Constraint;
class BranchRecord;
class ValueRecord;
class FunctionRecord;
class EqualsRecord;
class LoadRF;
class ScheduleBuilder;
class IncrementalSolver;

typedef unsigned int uint;
enum EventType {LOAD, STORE, RMW, BRANCHDIR, MERGE, FUNCTION, THREADCREATE, THREADBEGIN, NONLOCALTRANS, LOOPEXIT, LABEL,YIELD, THREADJOIN, LOOPENTER, LOOPSTART, ALLOC, EQUALS, FENCE};
typedef HashSet<EPRecord *, uintptr_t, 0, model_malloc, model_calloc, model_free> RecordSet;
typedef HSIterator<EPRecord *, uintptr_t, 0, model_malloc, model_calloc, model_free> RecordIterator;
typedef HashTable<EPRecord *, BranchRecord *, uintptr_t, 4, model_malloc, model_calloc, model_free> BranchTable;
typedef HashSet<EPRecord *, uintptr_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> SnapRecordSet;
typedef HSIterator<EPRecord *, uintptr_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free, default_hash_function<EPRecord *, 0, uintptr_t>, default_equals<EPRecord *> > SnapRecordIterator;
typedef HashTable<const void *, StoreLoadSet *, uintptr_t, 4, model_malloc, model_calloc, model_free> StoreLoadSetHashTable;
typedef HashTable<EPRecord *, RecordSet *, uintptr_t, 4, model_malloc, model_calloc, model_free> LoadHashTable;
typedef HashSet<uint64_t, uint64_t, 0, model_malloc, model_calloc, model_free> IntHashSet;
typedef HSIterator<uint64_t, uint64_t, 0, model_malloc, model_calloc, model_free> IntIterator;
typedef HashSet<uint64_t, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> SnapIntHashSet;
typedef HSIterator<uint64_t, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free> SnapIntIterator;
typedef HashTable<EPRecord *, Constraint **, uintptr_t, 4, model_malloc, model_calloc, model_free> VarTable;
typedef HashTable<EPRecord *, FunctionRecord *, uintptr_t, 4, model_malloc, model_calloc, model_free> FunctionTable;
typedef HashTable<EPRecord *, EqualsRecord *, uintptr_t, 4, model_malloc, model_calloc, model_free> EqualsTable;
#endif

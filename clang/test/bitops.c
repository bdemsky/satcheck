#include <stdbool.h>
#include "libinterface.h"

/* atomic */ int flag1, flag2;

#define true 1
#define false 0
#define NULL 0
#define MAX_FREELIST 4 /* Each thread can own up to MAX_FREELIST free nodes */
static unsigned int (*free_lists)[MAX_FREELIST];

typedef /* atomic */ unsigned long long pointer_t;
typedef unsigned long long pointer;
#define MAKE_POINTER(ptr, count)    ((((pointer)count) << 32) | ptr)
#define COUNT_MASK (0xffffffffLL << 32)
#define PTR_MASK 0xffffffffLL
static inline unsigned int get_count(MCID _mp, pointer p, MCID * retval) { *retval = MCID_NODEP;
return (p & COUNT_MASK) >> 32; }
static inline unsigned int get_ptr(MCID _mp, pointer p, MCID * retval) { *retval = MCID_NODEP;
return p & PTR_MASK; }
#define MAX_NODES			0xf

typedef struct node {
	unsigned int value;
	pointer_t next;
} node_t;

typedef struct {
	pointer_t top;
	node_t nodes[MAX_NODES + 1];
} stack_t;

static unsigned int new_node(MCID * retval)
{
	int i;
	int t = get_thread_num(); // should be thrd_current but that's not guaranteed to be an array index
	MC2_enterLoop();
	for (i = 0; i < MAX_FREELIST; i++) {
        void * _p1 = &free_lists[t][i]; MCID _mp1 = MC2_function_id(8, 2, sizeof(_p1), t, i, _p1);
		MCID _mnode; _mnode=MC2_nextOpLoad(_mp1); unsigned int node = load_64(p1);
		MCID _br0;
		
		int _cond0 = node;
		MCID _cond0_m = MC2_function_id(1, 1, sizeof(_cond0), _cond0, _mnode);
		if (_cond0) {
			_br0 = MC2_branchUsesID(_mnode, 1, 2, true);
			MC2_nextOpStore(_mfree_lists, MCID_NODEP);
			store_64(&free_lists[t][i], 0);
			*retval = _mnode;
			MC2_exitLoop();
			return node;
		}
	} else { _br0 = MC2_branchUsesID(_mnode, 0, 2, true);	MC2_merge(_br0);
	 }
MC2_exitLoop();

	*retval = MCID_NODEP;
	return 0;
}

void p0(MCID _ms, stack_t *s) {
	MCID _rv0;
	unsigned int nodeIdx = new_node(&_rv0);
	MCID _mnode; node_t *node = &s->nodes[nodeIdx]; // why is this not load_64'd?
	MCID _moldTop; _moldTop=MC2_nextOpLoadOffset(_ms, MC2_OFFSET(stack_t *, top)); pointer oldTop = load_64(&s->top);
	MCID _fn0; MCID _rv1;
	pointer newTop = MAKE_POINTER(nodeIdx, get_count(oldTop) + 1);
	_fn0 = MC2_function_id(4, 2, sizeof (newTop), (uint64_t)newTop, _moldTop, _rv0); 
	MC2_nextOpStoreOffset(_mnode, MC2_OFFSET(node_t *, next), _moldTop);
	store_64(&node->next, oldTop);
	MCID _rmw0 = MC2_nextRMWOffset(_ms, MC2_OFFSET(stack_t *, top), MCID_NODEP, _rv1);
	rmw_64(CAS, &s->top, &oldTop, newTop);
}

void p0_no_macros(MCID _ms, stack_t *s) {
	MCID _rv2;
	unsigned int nodeIdx = new_node(&_rv2);
	MCID _mnode; node_t *node = &s->nodes[nodeIdx];
	MCID _moldTop; _moldTop=MC2_nextOpLoadOffset(_ms, MC2_OFFSET(stack_t *, top)); pointer oldTop = load_64(&s->top);
	MCID _fn1; pointer newTop = (pointer) (((((oldTop & COUNT_MASK) >> 32) + 1) << 32) | nodeIdx);
	_fn1 = MC2_function_id(5, 2, sizeof (newTop), (uint64_t)newTop, _moldTop, _rv2); 
	MC2_nextOpStoreOffset(_mnode, MC2_OFFSET(node_t *, next), _moldTop);
	store_64(&node->next, oldTop);
	MCID _rmw1 = MC2_nextRMWOffset(_ms, MC2_OFFSET(stack_t *, top), MCID_NODEP, _fn1);
	rmw_64(CAS, &s->top, &oldTop, newTop);
}

int p1(MCID _ms, stack_t *s, MCID * retval) {
	MCID _mnext; MCID _fn2; MCID _moldTop; _moldTop=MC2_nextOpLoadOffset(_ms, MC2_OFFSET(stack_t *, top)); pointer oldTop = load_64(&s->top), newTop, next;
	node_t *node;
	MCID _br1;
	
	int _cond1 = get_ptr(oldTop) == 0;
	MCID _cond1_m = MC2_function_id(2, 1, sizeof(_cond1), _cond1, _moldTop);
	if (_cond1) {
		*retval = MCID_NODEP;
		_br1 = MC2_branchUsesID(_cond1_m, 1, 2, true);
		return 0; }
	 else { _br1 = MC2_branchUsesID(_cond1_m, 0, 2, true);	MC2_merge(_br1);
	 }MCID _rv3;
	node = &s->nodes[get_ptr(MCID_NODEP, oldTop, &_rv3)]; // why is this not load_64'd?
             
	_mnext=MC2_nextOpLoadOffset(_mnode, MC2_OFFSET(node_t *, next)), next = load_64(&node->next);
	MCID _rv5;
	MCID _rv4;
	newTop = MAKE_POINTER(get_ptr(next), get_count(oldTop) + 1);
    MCID _mnewTop = MC2_function_id(9, 2, sizeof (newTop), (uint64_t)newTop, get_ptr(next), get_count(oldTop) + 1);
             
	MCID _rmw2 = MC2_nextRMWOffset(_ms, MC2_OFFSET(stack_t *, top), MCID_NODEP, _fn2);
	rmw_64(CAS, &s->top, &oldTop, newTop);
	*retval = MCID_NODEP;
	return 0;
}

int p1_no_macros(MCID _ms, stack_t *s, MCID * retval) {
	MCID _mnext; MCID _fn3; MCID _moldTop; _moldTop=MC2_nextOpLoadOffset(_ms, MC2_OFFSET(stack_t *, top)); pointer oldTop = load_64(&s->top), newTop, next;
	node_t *node;
	MCID _br2;
	
	int _cond2 = (oldTop & 4294967295LL) == 0;
	MCID _cond2_m = MC2_function_id(3, 1, sizeof(_cond2), _cond2, _moldTop);
	if (_cond2) {
		*retval = MCID_NODEP;
		_br2 = MC2_branchUsesID(_cond2_m, 1, 2, true);
		return 0; }
	 else { _br2 = MC2_branchUsesID(_cond2_m, 0, 2, true);	MC2_merge(_br2);
	 }node = &s->nodes[oldTop & PTR_MASK];
             
	_mnext=MC2_nextOpLoadOffset(_mnode, MC2_OFFSET(node_t *, next)), next = load_64(&node->next);
	newTop = (pointer) (((((oldTop & COUNT_MASK) >> 32) + 1) << 32) | (next & PTR_MASK));
	_fn3 = MC2_function_id(7, 3, sizeof (newTop), (uint64_t)newTop, _fn3, _mnext, _moldTop); 
             
	MCID _rmw3 = MC2_nextRMWOffset(_ms, MC2_OFFSET(stack_t *, top), MCID_NODEP, _fn3);
	rmw_64(CAS, &s->top, &oldTop, newTop);
	*retval = MCID_NODEP;
	return 0;
}

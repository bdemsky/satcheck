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
static inline unsigned int get_count(pointer p) { return (p & COUNT_MASK) >> 32; }
static inline unsigned int get_ptr(pointer p) { return p & PTR_MASK; }
#define MAX_NODES			0xf

typedef struct node {
	unsigned int value;
	pointer_t next;
} node_t;

typedef struct {
	pointer_t top;
	node_t nodes[MAX_NODES + 1];
} stack_t;

static unsigned int new_node()
{
	int i;
	int t = get_thread_num();
	for (i = 0; i < MAX_FREELIST; i++) {
		unsigned int node = load_64(&free_lists[t][i]);
		if (node) {
			store_64(&free_lists[t][i], 0);
			return node;
		}
	}
	return 0;
}

void p0(stack_t *s) {
	unsigned int nodeIdx = new_node();
	node_t *node = &s->nodes[nodeIdx];
	pointer oldTop = load_64(&s->top);
	pointer newTop = MAKE_POINTER(nodeIdx, get_count(oldTop) + 1);
	store_64(&node->next, oldTop);
	rmw_64(CAS, &s->top, &oldTop, newTop);
}

void p0_no_macros(stack_t *s) {
	unsigned int nodeIdx = new_node();
	node_t *node = &s->nodes[nodeIdx];
	pointer oldTop = load_64(&s->top);
	pointer newTop = (pointer) (((((oldTop & COUNT_MASK) >> 32) + 1) << 32) | nodeIdx);
	store_64(&node->next, oldTop);
	rmw_64(CAS, &s->top, &oldTop, newTop);
}

int p1(stack_t *s) {
	pointer oldTop = load_64(&s->top), newTop, next;
	node_t *node;
	if (get_ptr(oldTop) == 0)
		return 0;
	node = &s->nodes[get_ptr(oldTop)];
             
	next = load_64(&node->next);
	newTop = MAKE_POINTER(get_ptr(next), get_count(oldTop) + 1);
             
	rmw_64(CAS, &s->top, &oldTop, newTop);
	return 0;
}

int p1_no_macros(stack_t *s) {
	pointer oldTop = load_64(&s->top), newTop, next;
	node_t *node;
	if ((oldTop & PTR_MASK) == 0)
		return 0;
	node = &s->nodes[oldTop & PTR_MASK];
             
	next = load_64(&node->next);
	newTop = (pointer) (((((oldTop & COUNT_MASK) >> 32) + 1) << 32) | (next & PTR_MASK));
             
	rmw_64(CAS, &s->top, &oldTop, newTop);
	return 0;
}

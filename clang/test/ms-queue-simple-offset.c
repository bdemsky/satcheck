#define PROBLEMSIZE 10
#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#define true 1
#define false 0
#define bool int

#include "ms-queue-simple.h"
#include "libinterface.h"

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads);
void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val);
bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal, MCID * retval);

queue_t queue;

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads) {
	MCID _fn0; struct node *newnode=malloc(sizeof (struct node));
	_fn0 = MC2_function(0, sizeof (newnode), newnode);
	MC2_nextOpStoreOffset(_fn0, MC2_OFFSET(struct node *, value), MCID_NODEP);
	store_32(&newnode->value, 0);

	MC2_nextOpStoreOffset(_fn0, MC2_OFFSET(struct node *, next), MCID_NODEP);
	store_64(&newnode->next, (uintptr_t) NULL);
		/* initialize queue */	
	MC2_nextOpStoreOffset(_mq, MC2_OFFSET(queue_t *, head), _fn0);
	store_64(&q->head, (uintptr_t) newnode);
	
	MC2_nextOpStoreOffset(_mq, MC2_OFFSET(queue_t *, tail), _fn0);
	store_64(&q->tail, (uintptr_t) newnode);
}

void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val) {
	MCID _mtail; struct node *tail;
	MCID _fn1; struct node * node_ptr = malloc(sizeof(struct node));
	_fn1 = MC2_function(0, sizeof (node_ptr), node_ptr);
	
	MC2_nextOpStoreOffset(_fn1, MC2_OFFSET(struct node *, value), _mval);
	store_32(&node_ptr->value, val);
	
	
	MC2_nextOpStoreOffset(_fn1, MC2_OFFSET(struct node *, next), MCID_NODEP);
	store_64(&node_ptr->next, (uintptr_t) NULL);
			
	MC2_enterLoop();
	while (true) {

		_mtail=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, tail)), tail = (struct node *) load_64(&q->tail);
		struct node ** tail_ptr_next= &tail->next;

		MCID _mnext=MC2_nextOpLoadOffset(_fn2, MC2_OFFSET(struct node *, next));
		struct node * next = (struct node *) load_64(tail_ptr_next);
		
		MCID _mqtail; 
		_mqtail=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *,tail)); struct node * qtail = (struct node *) load_64(&q->tail);

		MCID _fn16;
		int tqt = MC2_equals(_fn2, tail, _mqtail, qtail, &_fn16);
		MCID _br0;
		if (tqt) {

			_br0 = MC2_branchUsesID(_fn16, 1, 2, true);

			MCID _br1;
			if (next) {
				_br1 = MC2_branchUsesID(_mnext, 1, 2, true);
				MC2_yield();
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_mnext, 0, 2, true);
				MCID _rmw0 = MC2_nextRMWOffset(_fn2, MC2_OFFSET(struct node *, next), _mnext, _fn1);
				struct node * valread = (struct node *) rmw_64(CAS, tail_ptr_next, (uintptr_t) next, (uintptr_t) node_ptr);
				MCID _fn17;
				int vn = MC2_equals(_mnext, next, _rmw0, valread, &_fn17);
				MCID _br1;
				if (vn) {
					_br1 = MC2_branchUsesID(_fn17, 1, 2, true);
					break;
				} else { _br1 = MC2_branchUsesID(_fn17, 0, 2, true);		MC2_merge(_br1);
				}
			}
			MCID _mnew_tailptr = MC2_nextOpLoadOffset(_fn2, MC2_OFFSET(struct node *, next));
			struct node * new_tailptr = (struct node *)load_64( tail_ptr_next);
			MCID _rmw1 = MC2_nextRMW(MCID_NODEP, _fn2, _mnew_tailptr);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) new_tailptr);
			MC2_yield();
			MC2_merge(_br0);
		} else { 
			_br0 = MC2_branchUsesID(_fn16, 0, 2, true);
			MC2_yield();
			MC2_merge(_br0);
		}
	}
	MC2_exitLoop();
	
	MCID _rmw2 = MC2_nextRMW(MCID_NODEP, _fn2, _fn1);
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) node_ptr);
}

bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal) {
	MC2_enterLoop();
	while (true) {
		void * _p8 = &q->head;
		MCID _mhead=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, head));
		struct node * head = (struct node *) load_64(_p8);
		void * _p9 = &q->tail;
		MCID _mtail=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, tail));
		struct node * tail = (struct node *) load_64(_p9);
		void * _p10 = &head->next;
		MCID _mnext = MC2_nextOpLoadOffset(_mhead, MC2_OFFSET(struct node *, next));
		struct node * next = (struct node *) load_64(_p10);

		void * _p11 = &q->head;
		MCID _mload=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, head));
		struct node * headr = (struct node *) load_64(_p11);

		MCID _mt;
		int t = MC2_equals(_mhead, head, _mload, headr, &_mt);
		MCID _br0;

		if (t) {
			_br0 = MC2_branchUsesID(_mt, 1, 2, true);

			MCID _fn18;
			int ht = MC2_equals(_mhead, head, _mtail, tail, &_fn18);

			MCID _br1;
			
			if (ht) {
				_br1 = MC2_branchUsesID(_fn18, 1, 2, true);

				MCID _br2;
				if (next) {
					_br2 = MC2_branchUsesID(_mnext, 1, 2, true);
					MC2_yield();
					MC2_merge(_br2);
				} else {
					_br2 = MC2_branchUsesID(_mnext, 0, 2, true);
					MC2_exitLoop();
					return false; // NULL
				}
				MCID _rmw3 = MC2_nextRMW(MCID_NODEP, _mtail, _mnext);
				rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) next);
				MC2_yield();
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_fn18, 0, 2, true);
				void * nextval= &(next->value);
				MCID _rVal = MC2_nextOpLoadOffset(_mnext, MC2_OFFSET(struct node *, value));
				*retVal = load_32(&next->value);
				MCID _rmw4 = MC2_nextRMW(MCID_NODEP, _mhead, _mnext);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) next);
				MCID _fn19;
				int ohh = MC2_equals(_mhead, head, _rmw4, old_head, &_fn19);
				MCID _br6;

				if (ohh) {
					_br6 = MC2_branchUsesID(_fn19, 1, 2, true);
					MC2_exitLoop();
					return true;
				} else {
					_br6 = MC2_branchUsesID(_fn19, 0, 2, true);
					MC2_yield();
					MC2_merge(_br6);
				}
				MC2_merge(_br1);
			}
			MC2_merge(_br0);
		} else {
			_br0 = MC2_branchUsesID(_mt, 0, 2, true);
			MC2_yield();
			MC2_merge(_br0);
		}
	}
	MC2_exitLoop();
}

/* ms-queue-main */
bool succ1, succ2;

static void e(void *p) {
	int i;
	for(i=0;i<4;i++)
		enqueue(MCID_NODEP, &queue, MCID_NODEP, 1);
}

static void d(void *p) {
	unsigned int val;
	int i;
	for(i=0;i<PROBLEMSIZE;i++)
		dequeue(MCID_NODEP, &queue, MCID_NODEP, &val);
}

int user_main(int argc, char **argv)
{
	init_queue(MCID_NODEP, &queue, MCID_NODEP, 2);

	thrd_t t1,t2;
	thrd_create(&t1, e, NULL);
	thrd_create(&t2, d, NULL);

	thrd_join(t1);
	thrd_join(t2);

	return 0;
}

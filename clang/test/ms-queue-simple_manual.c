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
bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal);

queue_t queue;

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads) {
	MCID _fn6; struct node *newnode=malloc(sizeof (struct node));
	
	void * _p0 = &newnode->value;
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_32(_p0, 0);

	
	void * _p1 = &newnode->next;
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_64(_p1, (uintptr_t) NULL);
		/* initialize queue */
	
	void * _p2 = &q->head;
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_64(_p2, (uintptr_t) newnode);
	
	
	void * _p3 = &q->tail;
	MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
	store_64(_p3, (uintptr_t) newnode);
}

void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val) {
	MCID _fn8; struct node *tail;
	MCID _fn7; struct node * node_ptr = malloc(sizeof(struct node));
	_fn7 = MC2_function(0, sizeof (node_ptr), node_ptr); 
	
	void * _p4 = &node_ptr->value;
	MCID _fn4 = MC2_function(1, MC2_PTR_LENGTH, _p4, _fn7);
	MC2_nextOpStore(_fn4, _mval);
	store_32(_p4, val);
	
	
	void * _p5 = &node_ptr->next;
	MCID _fn5 = MC2_function(1, MC2_PTR_LENGTH, _p5, _fn7);
	MC2_nextOpStore(_fn5, MCID_NODEP);
	store_64(_p5, (uintptr_t) NULL);
			
	MC2_enterLoop();
	while (true) {
		_fn8=MC2_nextOpLoad(MCID_NODEP), tail = (struct node *) load_64(&q->tail);

		MCID _fn9; struct node ** tail_ptr_next= & tail->next;
		_fn9 = MC2_function(1, sizeof (tail_ptr_next), tail_ptr_next, _fn8); 

		MCID _mnext; _mnext=MC2_nextOpLoad(_fn9); struct node * next = (struct node *) load_64(tail_ptr_next);
		
		MCID _mqtail; _mqtail=MC2_nextOpLoad(MCID_NODEP); struct node * qtail = (struct node *) load_64(&q->tail);

		bool comp = (tail==qtail);
		MCID _comp = MC2_function(2, 4, comp, _fn8, _mqtail);
		MCID _br0;
		if (comp) {
			_br0 = MC2_branchUsesID(_comp, 1, 2, true);

			MCID _br1;
			if (next) {
				_br1 = MC2_branchUsesID(_mnext, 1, 2, true);
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_mnext, 0, 2, true);
				MCID _rmw0 = MC2_nextRMW(_fn9, _mnext, _fn7);
				struct node * valread = (struct node *) rmw_64(CAS, tail_ptr_next, (uintptr_t) next, (uintptr_t) node_ptr);
				bool comp2=(valread == next);
				MCID _comp2 = MC2_function(2, 4, comp2, _rmw0, _mnext);
				MCID _br2;
				if (comp2) {
					_br2 = MC2_branchUsesID(_comp2, 1, 2, true);
					break;
				} else {
					_br2 = MC2_branchUsesID(_comp2, 0, 2, true);
					MC2_merge(_br2);
				}
				MC2_merge(_br1);
			}
			MCID _mnew_tailptr; _mnew_tailptr=MC2_nextOpLoad(_fn9); struct node * new_tailptr = (struct node *)load_64( tail_ptr_next);
			MCID _rmw1 = MC2_nextRMW(MCID_NODEP, _fn8, _mnew_tailptr);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) new_tailptr);
			MC2_yield();
			MC2_merge(_br0);
		} else {
			_br0 = MC2_branchUsesID(_comp, 0, 2, true);
			MC2_yield();
			MC2_merge(_br0);
		}
	}
	MC2_exitLoop();
	
	MCID _rmw2 = MC2_nextRMW(MCID_NODEP, _fn8, _fn7);
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) node_ptr);
}

bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal) {
	MC2_enterLoop();
	while (true) {
		MCID _mhead; _mhead=MC2_nextOpLoad(MCID_NODEP); struct node * head = (struct node *) load_64(&q->head);
		MCID _mtail; _mtail=MC2_nextOpLoad(MCID_NODEP); struct node * tail = (struct node *) load_64(&q->tail);
		MCID _mnext; _mnext=MC2_nextOpLoad(MCID_NODEP); struct node * next = (struct node *) load_64(&head->next);
		
		MCID _mt; _mt=MC2_nextOpLoad(MCID_NODEP); int t = ((struct node *) load_64(&q->head)) == head;
		MCID _br0;

		if (t) {
			_br0 = MC2_branchUsesID(_mt, 1, 2, true);

			bool comp = head == tail;
			MCID _mcomp = MC2_function(2, 4, comp, _mhead, _mtail);
			MCID _br1;
			
			if (comp) {
				_br1 = MC2_branchUsesID(_mcomp, 1, 2, true);
				MCID _br2;
				if (next) {
					_br2 = MC2_branchUsesID(_mnext, 1, 2, true);
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
				_br1 = MC2_branchUsesID(_mcomp, 0, 2, true);
				void * nextval= &(next->value);
				MCID _mnextval = MC2_function(1, MC2_PTR_LENGTH, nextval, _mnext);
				MCID _rVal = MC2_nextOpLoad(_mnextval);
				*retVal = load_32(&next->value);
				MCID _rmw4 = MC2_nextRMW(MCID_NODEP, _mhead, _mnext);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) next);
				bool comp1 = old_head == head;
				MCID _mcomp1 = MC2_function(2, 4, comp1, _rmw4, _mhead);
				MCID _br2;

				if (comp1) {
					_br2 = MC2_branchUsesID(_mcomp1, 1, 2, true);
					MC2_exitLoop();
					return true;
				} else {
					_br2 = MC2_branchUsesID(_mcomp1, 0, 2, true);
					MC2_yield();
					MC2_merge(_br2);
				}
				MC2_merge(_br1);
			}
			MC2_merge(_br0);
		} else {
			_br0 = MC2_branchUsesID(_mt, 0, 2, true);
			MC2_merge(_br0);
		}
	}
	MC2_exitLoop();
	
}

/* ms-queue-main */
bool succ1, succ2;

static void e(void *p) {
	enqueue(MCID_NODEP, &queue, MCID_NODEP, 1);
}

static void d(void *p) {
	unsigned int val;
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
